#include <cstdlib>
#include <cinttypes>

#include <fstream>
#include <stdexcept>

#include <chrono>
#include <thread>
#include <unistd.h>

extern "C"
{
#include <jansson.h>

#include "utilities_functions.h"
#include "streaming_header.h"
}

#include "map_utilities.hpp"

#define LINUX
#include "ADQAPI.h"
#include "ADQ_descriptions.hpp"
#include "ADQ_utilities.hpp"
#include "ADQ36_FWDAQ.hpp"

#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// The dump of the default configuration has 47130 characters
#define ADQAPI_JSON_BUFFER_SIZE 65536

const unsigned int ABCD::ADQ36_FWDAQ::default_data_reading_timeout = 3000;

ABCD::ADQ36_FWDAQ::ADQ36_FWDAQ(void *adq, int num) : ABCD::Digitizer(),
                                                     adq_cu_ptr(adq),
                                                     adq_num(num)
{
    SetModel("ADQ36_FWDAQ");

    const std::string log_name = GetModel() + "::ADQ36_FWDAQ()";
    absp_logger_console->info("{}", log_name);

    SetEnabled(false);

    // This is reset only at the class creation because the timestamp seems to
    // be growing continuously even after starts or stops.
    timestamp_last = 0;
    timestamp_offset = 0;
    timestamp_overflows = 0;
    timestamp_bit_shift = 0;
}

//==============================================================================

ABCD::ADQ36_FWDAQ::~ADQ36_FWDAQ()
{
    const std::string log_name = GetName() + " " + GetModel() + "::~ADQ36_FWDAQ()";
    absp_logger_console->info("{}", log_name);
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::Initialize()
{
    std::string log_name = GetModel() + "::Initialize()";
    absp_logger_console->info("{}", log_name);

    // Initialize the variable holding the configuration parameters
    if (ADQ_InitializeParameters(adq_cu_ptr, adq_num,
                                 ADQ_PARAMETER_ID_TOP, &adq_parameters) != sizeof(adq_parameters))
    {
        absp_logger_error->error("{} Failed to initialize digitizer parameters;", log_name);

        return DIGITIZER_FAILURE;
    }

    SetName(adq_parameters.constant.serial_number);

    log_name = GetName() + std::string(" ") + log_name;

    SetChannelsNumber(adq_parameters.constant.nof_channels);

    absp_logger_console->info("{} Initialized board", log_name);
    absp_logger_console->info("{} Card name (serial number): {}; Product name: {}; Card option: {}; Motherboard option: {}; DNA: {};", log_name, GetName(), adq_parameters.constant.product_name, ADQ_GetCardOption(adq_cu_ptr, adq_num), adq_parameters.constant.product_options, adq_parameters.constant.dna);
    // absp_logger_console->info("{} USB address: {}; PCIe address: {};", log_name, ADQ_GetUSBAddress(adq_cu_ptr, adq_num), ADQ_GetPCIeAddress(adq_cu_ptr, adq_num));
    absp_logger_console->info("{} ADQAPI revision: {}; ", log_name, ADQAPI_GetRevisionString());

    absp_logger_console->info("{} Channels number: {};", log_name, GetChannelsNumber());
    absp_logger_console->info("{} Firmware type: {}; name: {}; revision: {}; customization: {}; part number: {};", log_name, adq_parameters.constant.firmware.type, adq_parameters.constant.firmware.name, adq_parameters.constant.firmware.revision, adq_parameters.constant.firmware.customization, adq_parameters.constant.firmware.part_number);

    std::string output_string(ADQAPI_JSON_BUFFER_SIZE, '\0');

    CHECKNEGATIVE(ADQ_GetStatusString(adq_cu_ptr, adq_num,
                                      ADQ_STATUS_ID_TEMPERATURE,
                                      const_cast<char *>(output_string.c_str()),
                                      ADQAPI_JSON_BUFFER_SIZE, 1));

    absp_logger_console->info("{} Temperature JSON status: {};", log_name, output_string);

    CHECKNEGATIVE(ADQ_GetStatusString(adq_cu_ptr, adq_num,
                                      ADQ_STATUS_ID_ACQUISITION,
                                      const_cast<char *>(output_string.c_str()),
                                      ADQAPI_JSON_BUFFER_SIZE, 1));

    absp_logger_console->info("{} Acquisition JSON status: {};", log_name, output_string);

    if (absp_logger_console->should_log(spdlog::level::debug))
    {
        char time_buffer[std::size("  yyyy-mm-ddThh:mm:ssZ+00:00  ")];
        time_string(time_buffer, std::size(time_buffer), NULL);

        std::string file_name("ADQ36_");
        file_name += GetName();
        file_name += "_default-config_";
        file_name += time_buffer;
        file_name += ".json";

        absp_logger_console->info("{} Saving digitizer parameters to file: {};", log_name, file_name);
        CHECKNEGATIVE(ADQ_InitializeParametersFilename(adq_cu_ptr, adq_num,
                                                       ADQ_PARAMETER_ID_TOP,
                                                       file_name.c_str(),
                                                       1));
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::Configure()
{
    const std::string log_name = GetName() + " " + GetModel() + "::Configure()";
    absp_logger_console->info("{} Configuring board;", log_name);

    if (!IsEnabled())
    {
        return DIGITIZER_SUCCESS; // if card is OFF return immediately
    }

    absp_logger_console->info("{} Validating parameters with ADQ_ValidateParameters();", log_name);

    const int vp_result = ADQ_ValidateParameters(adq_cu_ptr, adq_num,
                                                 reinterpret_cast<void *>(&adq_parameters));

    if (vp_result < 0)
    {
        absp_logger_error->error("{} Failure in parameters validation;", log_name);

        return ADQ_ADQ36_ERROR_PARAMETERS_VALIDATION;
    }

    absp_logger_console->info("{} Setting parameters with ADQ_SetParameters();", log_name);

    const int sp_result = ADQ_SetParameters(adq_cu_ptr, adq_num,
                                            reinterpret_cast<void *>(&adq_parameters));

    if (sp_result < 0)
    {
        absp_logger_error->error("{} Failure in setting parameters;", log_name);

        return ADQ_ADQ36_ERROR_CONFIGURATION;
    }

    // -------------------------------------------------------------------------
    //  Custom JRC-Geel firmware configuration
    // -------------------------------------------------------------------------
    // We write the registers only if the custom-firmware was enabled, otherwise
    // we risk to write over registers of standard firmwares that we are not
    // supposed to.
    if (custom_firmware_enabled)
    {
        CustomFirmwareSetMode(custom_firmware_mode);
        CustomFirmwareSetPulseLength(custom_firmware_pulse_length);
        CustomFirmwareEnable(custom_firmware_enabled);
    }

    // -------------------------------------------------------------------------
    //  Managing the native JSON configuration of the digitizer
    // -------------------------------------------------------------------------
    if (absp_logger_console->should_log(spdlog::level::debug))
    {
        char time_buffer[std::size("  yyyy-mm-ddThh:mm:ssZ+00:00  ")];
        time_string(time_buffer, std::size(time_buffer), NULL);

        std::string file_name("ADQ36_");
        file_name += GetName();
        file_name += "_post-config_";
        file_name += time_buffer;
        file_name += ".json";

        absp_logger_console->debug("{} Saving digitizer parameters after configuration to file: {};", log_name, file_name);
        CHECKNEGATIVE(ADQ_GetParametersFilename(adq_cu_ptr, adq_num,
                                                ADQ_PARAMETER_ID_TOP,
                                                file_name.c_str(),
                                                1));
    }

    if (!native_config.empty())
    {
        absp_logger_console->info("{} Setting parameters from native configuration in the user configuration with ADQ_SetParameters();", log_name);

        CHECKNEGATIVE(ADQ_SetParametersString(adq_cu_ptr, adq_num,
                                              native_config.c_str(), native_config.length()));

        if (absp_logger_console->should_log(spdlog::level::debug))
        {
            char time_buffer[std::size("  yyyy-mm-ddThh:mm:ssZ+00:00  ")];
            time_string(time_buffer, std::size(time_buffer), NULL);

            std::string file_name("ADQ36_");
            file_name += GetName();
            file_name += "_post-native-config_";
            file_name += time_buffer;
            file_name += ".json";

            absp_logger_console->debug("{} Saving digitizer parameters after native configuration to file: {};", log_name, file_name);
            CHECKNEGATIVE(ADQ_GetParametersFilename(adq_cu_ptr, adq_num,
                                                    ADQ_PARAMETER_ID_TOP,
                                                    file_name.c_str(),
                                                    1));
        }
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

void ABCD::ADQ36_FWDAQ::SetChannelsNumber(unsigned int n)
{
    const std::string log_name = GetName() + " " + GetModel() + "::SetChannelsNumber()";
    absp_logger_console->debug("{} Setting channels number to: {};", log_name, n);

    Digitizer::SetChannelsNumber(n);
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::StartAcquisition()
{
    const std::string log_name = GetName() + " " + GetModel() + "::StartAcquisition()";
    absp_logger_console->trace("{} Starting acquisition;", log_name);

    // last_buffer_ready = std::chrono::system_clock::now();

    const int retval = ADQ_StartDataAcquisition(adq_cu_ptr, adq_num);

    if (retval != ADQ_EOK)
    {
        absp_logger_error->error("{} Unable to start acquisition (code: {}, {});", log_name, retval, ADQ_descriptions::error.at(retval));

        return DIGITIZER_FAILURE;
    }

    if (using_software_trigger)
    {
        // We need to set it at least to 1 to force at least one trigger
        int max_records_number = 1;

        for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++)
        {
            if (adq_parameters.acquisition.channel[channel].nof_records > max_records_number)
            {
                max_records_number = adq_parameters.acquisition.channel[channel].nof_records;
            }
        }

        for (int i = 0; i < max_records_number; i++)
        {
            ForceSoftwareTrigger();
        }
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::RearmTrigger()
{
    const std::string log_name = GetName() + " " + GetModel() + "::RearmTrigger()";
    absp_logger_console->trace("{} Rearming trigger;", log_name);

    if (using_software_trigger)
    {
        absp_logger_console->trace("{} Rearming trigger; Forcing software trigger;", log_name);

        ForceSoftwareTrigger();
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

bool ABCD::ADQ36_FWDAQ::AcquisitionReady()
{
    // For this streaming generation there is no information whether data
    // is available or not, we can only try to read it
    return true;
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::GetWaveformsFromCard(std::vector<struct event_waveform> &waveforms)
{
    const std::string log_name = GetName() + " " + GetModel() + "::DataOverflow()";

    const std::chrono::time_point<std::chrono::system_clock> waveforms_reading_start = std::chrono::system_clock::now();
    auto delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - waveforms_reading_start);

    int64_t available_records = ADQ_EAGAIN;

    do
    {
        int available_channel = ADQ_ANY_CHANNEL;
        struct ADQGen4RecordArray *ADQ_records_array = NULL;
        struct ADQDataReadoutStatus ADQ_status;

        // Using a zero here should make the function return immediately
        const int timeout = 0;

        available_records = ADQ_WaitForRecordBuffer(adq_cu_ptr, adq_num,
                                                    &available_channel,
                                                    reinterpret_cast<void **>(&ADQ_records_array),
                                                    timeout,
                                                    &ADQ_status);

        if (available_records == 0)
        {
            // This is a status only record, I am not sure how to handle it
            if (absp_logger_console->should_log(spdlog::level::info))
            {
                std::string status_flag;
                if (ADQ_status.flags == ADQ_DATA_READOUT_STATUS_FLAGS_STARVING)
                {
                    status_flag = "STARVING";
                }
                else if (ADQ_status.flags == ADQ_DATA_READOUT_STATUS_FLAGS_INCOMPLETE)
                {
                    status_flag = "INCOMPLETE";
                }
                else if (ADQ_status.flags == ADQ_DATA_READOUT_STATUS_FLAGS_DISCARDED)
                {
                    status_flag = "DISCARDED";
                }
                else
                {
                    status_flag = "UNKNOWN";
                }
                absp_logger_console->info("{} Status only record; Available channel: {}; Flags: {:08x} \"{}\";", log_name, available_channel, static_cast<unsigned int>(ADQ_status.flags), status_flag);
            }
        }
        else if (available_records == ADQ_EAGAIN)
        {
            // This is a safe timeout, it means that data is not yet
            // available
        }
        else if (available_records < 0 && available_records != ADQ_EAGAIN)
        {
            // This is an error!
            // The record should not have been allocated and the docs
            // suggest to stop the acquisition to understand the error value
            absp_logger_error->warn("{} Error code: {};", log_name, (long)available_records);
            return DIGITIZER_FAILURE;
        }
        else if (available_records > 0)
        {
            absp_logger_console->trace("{} Available for channel {}: records: {};", log_name, available_channel, (long)available_records);

            for (int64_t record_index = 0; record_index < available_records; record_index++)
            {

                // At this point the ADQ_record variable should be ready...
                const uint8_t channel = ADQ_records_array->record[record_index]->header->channel;
                const uint32_t record_number = ADQ_records_array->record[record_index]->header->record_number;
                const uint32_t samples_per_record = ADQ_records_array->record[record_index]->header->record_length;
                // Timestamp of the waveform's begin
                const uint64_t timestamp_begin = ADQ_records_array->record[record_index]->header->timestamp;
                // Timestamp of the start of the record relative to the threshold crossing sample
                // These two numbers might have different time resolutions
                // (even if they have the same time unit)
                const int64_t record_start = ADQ_records_array->record[record_index]->header->record_start;
                const uint64_t timestamp = timestamp_begin + record_start;

                absp_logger_console->trace("{} Channel {}: Number of collected records: {}; Record length: {};", log_name, channel, (unsigned long)record_number, (unsigned long)samples_per_record);

                // If we see a jump backward in timestamp bigger than half the timestamp
                // range we assume that there was an overflow in the timestamp counter.
                // A smaller jump could mean that the records in the buffer are not
                // sorted according to the timestamp, that would make sense but we have
                // not verified it.
                const int64_t timestamp_negative_difference = timestamp_last - timestamp;

                if (timestamp_negative_difference > ADQ36_FWDAQ_TIMESTAMP_THRESHOLD)
                {
                    absp_logger_error->warn("{} Detected timestamp overflow; Overflows: {}; Negative difference: {};", log_name, timestamp_overflows, (long long)timestamp_negative_difference);

                    timestamp_offset += ADQ36_FWDAQ_TIMESTAMP_MAX;
                    timestamp_overflows += 1;
                }

                // We do not add the offset here because we want to check the digitizer
                // behaviour and not the correction.
                timestamp_last = timestamp;

                const uint64_t timestamp_waveform = (timestamp + timestamp_offset) << timestamp_bit_shift;

                struct event_waveform this_waveform = waveform_create(timestamp_waveform,
                                                                      channel,
                                                                      samples_per_record,
                                                                      0);

                uint16_t *const samples = waveform_samples_get(&this_waveform);

                if (ADQ_records_array->record[record_index]->header->data_format == ADQ_DATA_FORMAT_INT16)
                {
                    const int16_t *data_buffer_16bits = reinterpret_cast<int16_t *>(ADQ_records_array->record[record_index]->data);

                    for (unsigned int sample_index = 0; sample_index < samples_per_record; sample_index++)
                    {
                        const int16_t value = data_buffer_16bits[sample_index];

                        // We add an offset to match it to the rest of ABCD
                        // WARNING: The 12 bit data is aligned to the MSB
                        // Signal processing modules in the data path utilize
                        // the full bit range for calculations. Therefore,
                        // ignoring the least significant bits in the output can
                        // result in a loss of accuracy.
                        samples[sample_index] = ((value) + (1 << 15));
                    }
                }
                else if (ADQ_records_array->record[record_index]->header->data_format == ADQ_DATA_FORMAT_INT32)
                {
                    absp_logger_error->warn("{} Data format is set to 32 bits;", log_name);

                    const int32_t *data_buffer_32bits = reinterpret_cast<int32_t *>(ADQ_records_array->record[record_index]->data);

                    for (unsigned int sample_index = 0; sample_index < samples_per_record; sample_index++)
                    {
                        const int16_t value = data_buffer_32bits[sample_index];

                        // We add an offset to match it to the rest of ABCD
                        samples[sample_index] = ((value) + (1 << 15));
                    }
                }
                else
                {
                    absp_logger_error->error("{} Unexpected data format (got: {});", log_name, ADQ_records_array->record[record_index]->header->data_format);
                }

                waveforms.push_back(this_waveform);
            }

            // ReturnRecordBuffer() signals to the API that we are done with the
            // record memory and it can be used again for another record.
            CHECKNEGATIVE(ADQ_ReturnRecordBuffer(adq_cu_ptr, adq_num,
                                                 available_channel,
                                                 ADQ_records_array));
        }

        delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - waveforms_reading_start);

        absp_logger_console->debug("{} Time since the last buffer ready: {}", log_name, delta_time.count());
    } while ((available_records >= 0) && (delta_time < std::chrono::milliseconds(data_reading_timeout)));

    if ((available_records >= 0) && (delta_time > std::chrono::milliseconds(data_reading_timeout)))
    {
        absp_logger_error->warn("{} Timeout in the data reading;", log_name);
    }

    absp_logger_console->info("{} Converted all samples; Timestamp overflows: {};", log_name, timestamp_overflows);

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::StopAcquisition()
{
    const std::string log_name = GetName() + " " + GetModel() + "::StopAcquisition()";

    const int retval = ADQ_StopDataAcquisition(adq_cu_ptr, adq_num);

    if (retval != ADQ_EOK && retval != ADQ_EINTERRUPTED)
    {
        absp_logger_error->error("{} FWPD functions; Stop acquisition error (code: {}, {});", log_name, retval, ADQ_descriptions::error.at(retval));

        return DIGITIZER_FAILURE;
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::ForceSoftwareTrigger()
{
    const std::string log_name = GetName() + " " + GetModel() + "::ForceSoftwareTrigger()";

    absp_logger_console->debug("{} Forcing a software trigger;", log_name);

    CHECKNEGATIVE(ADQ_SWTrig(adq_cu_ptr, adq_num));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::ReadConfig(json_t *config)
{
    const std::string log_name = GetName() + " " + GetModel() + "::ReadConfig()";

    absp_logger_console->info("{} Reading configuration JSON;", log_name);

    const bool enable = json_is_true(json_object_get(config, "enable"));
    SetEnabled(enable);

    // Set again the settings that was just read, so in the case it was not present
    // the configuration becomes correct
    json_object_set_nocheck(config, "enable", json_boolean(enable));

    absp_logger_console->info("{} Card is {};", log_name, (enable ? "enabled" : "disabled"));

    // -------------------------------------------------------------------------
    //  Reading the ABCD-style configuration
    // -------------------------------------------------------------------------
    using_software_trigger = false;

    // -------------------------------------------------------------------------
    //  Custom JRC-Geel firmware configuration
    // -------------------------------------------------------------------------
    json_t *json_custom_firmware = json_object_get(config, "JRC_custom_firmware");

    if (json_custom_firmware != NULL && json_is_object(json_custom_firmware))
    {
        absp_logger_error->warn("{} JRC-Geel custom firmware settings detected;", log_name);

        custom_firmware_enabled = json_is_true(json_object_get(json_custom_firmware, "enable"));
        absp_logger_console->info("{} Custom firmware settings are {};", log_name, (custom_firmware_enabled ? "enabled" : "disabled"));

        if (custom_firmware_enabled)
        {
            custom_firmware_pulse_length = json_number_value(json_object_get(json_custom_firmware, "pulse_length"));
            custom_firmware_mode = json_number_value(json_object_get(json_custom_firmware, "mode"));
        }
    }

    // -------------------------------------------------------------------------
    //  Clock system part
    // -------------------------------------------------------------------------
    struct ADQClockSystemParameters clock_system;
    if (ADQ_InitializeParameters(adq_cu_ptr, adq_num,
                                 ADQ_PARAMETER_ID_CLOCK_SYSTEM,
                                 &clock_system) != sizeof(clock_system))
    {
        absp_logger_error->error("{} Failed to initialize clock_system parameters;", log_name);

        return DIGITIZER_FAILURE;
    }

    json_t *json_clock = json_object_get(config, "clock");

    if (!json_clock)
    {
        absp_logger_error->error("{} Missing \"clock\" entry in configuration;", log_name);

        json_clock = json_object();

        json_object_set_new_nocheck(config, "clock", json_clock);
    }

    if (json_clock != NULL && json_is_object(json_clock))
    {
        const char *cstr_clock_source = json_string_value(json_object_get(json_clock, "source"));
        const std::string str_clock_source = (cstr_clock_source) ? std::string(cstr_clock_source) : std::string();

        // Looking for the settings in the description map
        const auto cs_result = map_utilities::find_item(ADQ_descriptions::ADQ36_clock_source, str_clock_source);

        if (cs_result != ADQ_descriptions::ADQ36_clock_source.end() && str_clock_source.length() > 0)
        {
            clock_system.reference_source = cs_result->first;
        }
        else
        {
            clock_system.reference_source = ADQ_REFERENCE_CLOCK_SOURCE_INTERNAL;

            absp_logger_error->error("{} Wrong clock source;", log_name);
        }

        // If the clock source is set to external, then the clock generator
        // should be set to external as well... maybe
        // if (clock_system.reference_source == ADQ_REFERENCE_CLOCK_SOURCE_INTERNAL) {
        //    clock_system.clock_generator = ADQ_CLOCK_GENERATOR_INTERNAL_PLL;
        //} else {
        //    clock_system.clock_generator = ADQ_CLOCK_GENERATOR_EXTERNAL_CLOCK;
        //}

        absp_logger_console->info("{} Setting clock; clock_source: {};", log_name, ADQ_descriptions::ADQ36_clock_source.at(clock_system.reference_source));
        json_object_set_nocheck(json_clock, "source", json_string(ADQ_descriptions::ADQ36_clock_source.at(clock_system.reference_source).c_str()));

        clock_system.reference_frequency = json_number_value(json_object_get(json_clock, "reference_frequency"));

        clock_system.low_jitter_mode_enabled = 1;
    }

    const int Scs_result = ADQ_SetParameters(adq_cu_ptr, adq_num, reinterpret_cast<void *>(&clock_system));
    if (Scs_result < 0)
    {
        absp_logger_error->error("{} Failed to set clock_system parameters;", log_name);

        return DIGITIZER_FAILURE;
    }

    data_reading_timeout = json_number_value(json_object_get(config, "data_reading_timeout"));

    if (data_reading_timeout <= 0)
    {
        data_reading_timeout = default_data_reading_timeout;
    }

    absp_logger_console->info("{} Data reading timeout: {} ms;", log_name, data_reading_timeout);
    json_object_set_nocheck(config, "data_reading_timeout", json_integer(data_reading_timeout));

    timestamp_bit_shift = json_number_value(json_object_get(config, "timestamp_bit_shift"));
    absp_logger_console->info("{} Timestamp bit shift: {} bit;", log_name, timestamp_bit_shift);
    json_object_set_nocheck(config, "timestamp_bit_shift", json_integer(timestamp_bit_shift));

    // -------------------------------------------------------------------------
    //  Reading the ports configuration
    // -------------------------------------------------------------------------

    json_t *json_ports = json_object_get(config, "ports");

    if (json_ports != NULL && json_is_array(json_ports))
    {
        size_t index;
        json_t *value;

        json_array_foreach(json_ports, index, value)
        {
            json_t *json_id = json_object_get(value, "id");

            if (json_id != NULL && json_is_string(json_id))
            {
                const char *cstr_id = json_string_value(json_id);
                const std::string str_id = cstr_id ? std::string(cstr_id) : std::string("");

                absp_logger_console->info("{} Found port: {};", log_name, str_id);

                // Looking for the settings in the description map
                const auto pi_result = map_utilities::find_item(ADQ_descriptions::ADQ36_port_ids, str_id);

                enum ADQParameterId id = ADQ_PARAMETER_ID_RESERVED;

                if (pi_result != ADQ_descriptions::ADQ36_port_ids.end() && str_id.length() > 0)
                {
                    id = pi_result->first;
                }
                else
                {
                    id = ADQ_PARAMETER_ID_RESERVED;

                    absp_logger_error->error("{} Invalid port id (got: {});", log_name, str_id);
                }

                if (id != ADQ_PARAMETER_ID_RESERVED)
                {
                    const char *cstr_impedance = json_string_value(json_object_get(value, "input_impedance"));
                    const std::string str_impedance = (cstr_impedance) ? std::string(cstr_impedance) : std::string();

                    // Looking for the settings in the description map
                    const auto ii_result = map_utilities::find_item(ADQ_descriptions::input_impedance, str_impedance);

                    enum ADQImpedance impedance = ADQ_IMPEDANCE_HIGH;

                    if (ii_result != ADQ_descriptions::input_impedance.end() && str_impedance.length() > 0)
                    {
                        impedance = ii_result->first;
                    }
                    else
                    {
                        impedance = ADQ_IMPEDANCE_HIGH;

                        absp_logger_error->error("{} Invalid port impedance (got: {});", log_name, str_impedance);
                    }

                    absp_logger_console->info("{} Input impedance: {};", log_name, str_impedance);
                    json_object_set_nocheck(value, "input_impedance",
                                            json_string(ADQ_descriptions::input_impedance.at(impedance).c_str()));

                    const char *cstr_direction = json_string_value(json_object_get(value, "direction"));
                    const std::string str_direction = (cstr_direction) ? std::string(cstr_direction) : std::string();

                    // Looking for the settings in the description map
                    const auto pd_result = map_utilities::find_item(ADQ_descriptions::pin_direction, str_direction);

                    enum ADQDirection direction = ADQ_DIRECTION_IN;

                    if (pd_result != ADQ_descriptions::pin_direction.end() && str_direction.length() > 0)
                    {
                        direction = pd_result->first;
                    }
                    else
                    {
                        direction = ADQ_DIRECTION_IN;

                        absp_logger_error->error("{} Invalid pin direction (got: {});", log_name, str_direction);
                    }

                    absp_logger_console->info("{} Pin direction: {};", log_name, str_direction);
                    json_object_set_nocheck(value, "direction",
                                            json_string(ADQ_descriptions::pin_direction.at(direction).c_str()));

                    const char *cstr_function = json_string_value(json_object_get(value, "function"));
                    const std::string str_function = (cstr_function) ? std::string(cstr_function) : std::string();

                    const auto pf_result = map_utilities::find_item(ADQ_descriptions::pin_function, str_function);

                    enum ADQFunction function = ADQ_FUNCTION_GPIO;

                    if (pf_result != ADQ_descriptions::pin_function.end() && str_function.length() > 0)
                    {
                        function = pf_result->first;
                    }
                    else
                    {
                        function = ADQ_FUNCTION_GPIO;

                        absp_logger_error->error("{} Invalid pin function (got: {});", log_name, str_function);
                    }

                    absp_logger_console->info("{} Pin function: {};", log_name, str_function);
                    json_object_set_nocheck(value, "function",
                                            json_string(ADQ_descriptions::pin_function.at(function).c_str()));

                    struct ADQPortParameters port_parameters;

                    int result = ADQ_InitializeParameters(adq_cu_ptr, adq_num, id, &port_parameters);
                    if (result != sizeof(port_parameters))
                    {
                        absp_logger_error->error("{} Failed to initialize port parameters;", log_name);

                        return DIGITIZER_FAILURE;
                    }

                    port_parameters.pin[0].input_impedance = impedance;
                    port_parameters.pin[0].direction = direction;
                    port_parameters.pin[0].function = function;
                    port_parameters.pin[0].invert_output = 0;
                    port_parameters.pin[0].value = 0;

                    result = ADQ_SetParameters(adq_cu_ptr, adq_num, &port_parameters);
                    if (result != sizeof(port_parameters))
                    {
                        absp_logger_error->error("{} Failed to set port parameters;", log_name);

                        return DIGITIZER_FAILURE;
                    }
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    //  Reading the single channels configuration
    // -------------------------------------------------------------------------
    // First resetting the channels statuses
    for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++)
    {
        SetChannelEnabled(channel, false);
    }

    // Then reading the updated parameters from the board
    if (ADQ_GetParameters(adq_cu_ptr, adq_num,
                          ADQ_PARAMETER_ID_TOP, &adq_parameters) != sizeof(adq_parameters))
    {
        absp_logger_error->error("{} Failed to initialize digitizer parameters;", log_name);

        return DIGITIZER_FAILURE;
    }

    // Finally reading the channels configuration
    json_t *json_channels = json_object_get(config, "channels");

    if (json_channels != NULL && json_is_array(json_channels))
    {
        size_t index;
        json_t *value;

        json_array_foreach(json_channels, index, value)
        {
            json_t *json_id = json_object_get(value, "id");

            if (json_id != NULL && json_is_integer(json_id))
            {
                const int id = json_integer_value(json_id);

                absp_logger_console->info("{} Found channel: {};", log_name, id);

                const bool enabled = json_is_true(json_object_get(value, "enable"));

                absp_logger_console->info("{} Channel is {};", log_name, (enabled ? "disabled" : "enabled"));

                json_object_set_new_nocheck(value, "enable", json_boolean(enabled));

                // -------------------------------------------------------------
                //  Analog front end settings (AFE)
                // -------------------------------------------------------------

                // This is not available in the ADQ36 that we have
                // double input_range = json_number_value(json_object_get(value, "input_range"));
                // if (input_range <= 0) {
                //    input_range = default_input_range;
                //    absp_logger_error->error("{} Input range must be greater than zero, got: {};", log_name, str_sync_impedance);
                //}

                // absp_logger_console->info("{} Input range {} mVpp;", log_name, input_range);
                // json_object_set_nocheck(value, "input_range", json_real(input_range));

                const int DC_offset = json_number_value(json_object_get(value, "DC_offset"));
                absp_logger_console->info("{} DC offset {} samples;", log_name, DC_offset);
                json_object_set_nocheck(value, "DC_offset", json_integer(DC_offset));

                // -------------------------------------------------------------
                //  Signal processing parameters
                // -------------------------------------------------------------
                const bool DBS_enable = json_is_true(json_object_get(value, "DBS_enable"));
                absp_logger_console->info("{} DBS enable: {};", log_name, (DBS_enable ? "true" : "false"));
                json_object_set_nocheck(value, "DBS_enable", DBS_enable ? json_true() : json_false());

                const int64_t DBS_level = json_number_value(json_object_get(value, "DBS_level"));
                absp_logger_console->info("{} DBS level: {};", log_name, DBS_level);
                json_object_set_nocheck(value, "DBS_level", json_integer(DBS_level));

                const int raw_skip_factor = json_number_value(json_object_get(value, "skip_factor"));
                const int64_t skip_factor = (raw_skip_factor < 1) ? 1 : raw_skip_factor;
                absp_logger_console->info("{} Skip factor: {};", log_name, skip_factor);
                json_object_set_nocheck(value, "skip_factor", json_integer(skip_factor));

                // -------------------------------------------------------------
                //  Event source parameters (triggering)
                // -------------------------------------------------------------
                const char *cstr_trigger_source = json_string_value(json_object_get(value, "trigger_source"));
                const std::string str_trigger_source = (cstr_trigger_source) ? std::string(cstr_trigger_source) : std::string();

                // Looking for the settings in the description map
                const auto tsr_result = map_utilities::find_item(ADQ_descriptions::event_source, str_trigger_source);

                enum ADQEventSource trigger_source = ADQ_EVENT_SOURCE_LEVEL;

                if (tsr_result != ADQ_descriptions::event_source.end() && str_trigger_source.length() > 0)
                {
                    trigger_source = tsr_result->first;
                }
                else
                {
                    trigger_source = ADQ_EVENT_SOURCE_LEVEL;

                    absp_logger_error->error("{} Invalid trigger source (got: {});", log_name, str_trigger_source);
                }

                absp_logger_console->info("{} Trigger source: {};", log_name, str_trigger_source);
                json_object_set_nocheck(value, "trigger_source",
                                        json_string(ADQ_descriptions::event_source.at(trigger_source).c_str()));

                const int64_t raw_trigger_level = json_number_value(json_object_get(value, "trigger_level"));
                absp_logger_console->info("{} Trigger level: {};", log_name, raw_trigger_level);
                json_object_set_nocheck(value, "trigger_level", json_integer(raw_trigger_level));

                // This level should be the absolute trigger level.
                // In the rest of ABCD the waveforms' samples are treated as uint16_t and we
                // are offsetting what we read from the ABCD::ADQ36_FWDAQ to convert from int16_t.
                // The user should be able to set a trigger level according to what it is
                // shown in the waveforms display, thus we should expect a uin16_t number
                // that we convert to a int16_t.
                const int64_t trigger_level = (raw_trigger_level - 0x7FFF);

                const int64_t trigger_hysteresis = json_number_value(json_object_get(value, "trigger_hysteresis"));
                absp_logger_console->info("{} Trigger hysteresis: {};", log_name, trigger_hysteresis);
                json_object_set_nocheck(value, "trigger_hysteresis", json_integer(trigger_hysteresis));

                const char *cstr_trigger_slope = json_string_value(json_object_get(value, "trigger_slope"));
                const std::string str_trigger_slope = (cstr_trigger_slope) ? std::string(cstr_trigger_slope) : std::string();

                // Looking for the settings in the description map
                const auto tsl_result = map_utilities::find_item(ADQ_descriptions::slope, str_trigger_slope);

                enum ADQEdge trigger_slope = ADQ_EDGE_FALLING;

                if (tsl_result != ADQ_descriptions::slope.end() && str_trigger_slope.length() > 0)
                {
                    trigger_slope = tsl_result->first;
                }
                else
                {
                    trigger_slope = ADQ_EDGE_FALLING;

                    absp_logger_error->error("{} Invalid trigger slope, got: {};", log_name, str_trigger_slope);
                }

                absp_logger_console->info("{} Trigger slope: {};", log_name, ADQ_descriptions::trigger_slope.at(trigger_slope));
                json_object_set_nocheck(value, "trigger_slope", json_string(ADQ_descriptions::trigger_slope.at(trigger_slope).c_str()));

                const int raw_pretrigger = json_number_value(json_object_get(value, "pretrigger"));

                const int64_t pretrigger = std::ceil(raw_pretrigger / ADQ_ADQ36_SAMPLES_RESOLUTION) * ADQ_ADQ36_SAMPLES_RESOLUTION;

                absp_logger_console->info("{} Pretrigger: {};", log_name, pretrigger);
                json_object_set_nocheck(value, "pretrigger", json_integer(pretrigger));

                unsigned int raw_scope_samples = json_number_value(json_object_get(value, "scope_samples"));
                raw_scope_samples = (raw_scope_samples < ADQ_ADQ36_MINIMUM_SCOPE_SAMPLES) ? ADQ_ADQ36_MINIMUM_SCOPE_SAMPLES : raw_scope_samples;
                const int64_t scope_samples = std::ceil(raw_scope_samples / ADQ_ADQ36_SAMPLES_RESOLUTION) * ADQ_ADQ36_SAMPLES_RESOLUTION;

                absp_logger_console->info("{} Scope samples: {};", log_name, scope_samples);
                json_object_set_nocheck(value, "scope_samples", json_integer(scope_samples));

                const unsigned int raw_records_per_buffer = json_number_value(json_object_get(value, "records_per_buffer"));

                const unsigned int records_per_buffer = (raw_records_per_buffer < 1) ? 1 : raw_records_per_buffer;

                absp_logger_console->info("{} Records per buffer: {};", log_name, records_per_buffer);
                json_object_set_nocheck(value, "records_per_buffer", json_integer(records_per_buffer));

                // This would limit the number of records read in an acquisition.
                // We hide this setting from the user because it can be confusing.
                // We will just use an infinite number of records in an acquisition.
                // const int raw_records_number = json_number_value(json_object_get(value, "records_number"));
                // const int records_number = (raw_records_number < 1) ? ADQ_INFINITE_NOF_RECORDS : raw_records_number;
                // json_object_set_nocheck(value, "records_number", json_integer(records_number));

                // -------------------------------------------------------------------------
                //  Setting the channel configuration
                // -------------------------------------------------------------------------
                if (0 <= id && id < static_cast<int>(GetChannelsNumber()))
                {
                    SetChannelEnabled(id, enabled);

                    // Required to see ADC data
                    adq_parameters.test_pattern.channel[id].source = ADQ_TEST_PATTERN_SOURCE_DISABLE;

                    adq_parameters.afe.channel[id].dc_offset = DC_offset;
                    // This is not available in the ADQ36 that we have
                    // adq_parameters.afe.channel[id].input_range = input_range;

                    adq_parameters.signal_processing.dbs.channel[id].enabled = DBS_enable;
                    adq_parameters.signal_processing.dbs.channel[id].level = DBS_enable;

                    adq_parameters.signal_processing.sample_skip.channel[id].skip_factor = skip_factor;

                    adq_parameters.acquisition.channel[id].trigger_source = trigger_source;
                    adq_parameters.acquisition.channel[id].trigger_edge = trigger_slope;
                    adq_parameters.acquisition.channel[id].horizontal_offset = -1 * pretrigger;
                    adq_parameters.acquisition.channel[id].record_length = scope_samples;
                    adq_parameters.acquisition.channel[id].nof_records = ADQ_INFINITE_NOF_RECORDS;

                    // According to the SPD support, setting nof_records to zero
                    // will disable any data from the channel
                    if (!enabled)
                    {
                        adq_parameters.acquisition.channel[id].nof_records = 0;
                    }

                    adq_parameters.event_source.level.channel[id].level = trigger_level;
                    adq_parameters.event_source.level.channel[id].arm_hysteresis = trigger_hysteresis;

                    const int64_t bytes_per_sample = std::ceil(adq_parameters.acquisition.channel[id].nof_bits_per_sample / 8);

                    adq_parameters.transfer.channel[id].record_size = bytes_per_sample * scope_samples;
                    adq_parameters.transfer.channel[id].record_buffer_size = bytes_per_sample * scope_samples * records_per_buffer;
                    adq_parameters.transfer.channel[id].metadata_buffer_size = sizeof(struct ADQGen4RecordHeader) * records_per_buffer;
                    adq_parameters.transfer.channel[id].infinite_record_length_enabled = 0;
                    adq_parameters.transfer.channel[id].metadata_enabled = 1;
                    // Using the maximum value here, we have no reason to use a smaller value
                    adq_parameters.transfer.channel[id].nof_buffers = ADQ_MAX_NOF_BUFFERS;

                    adq_parameters.readout.channel[id].nof_record_buffers_in_array = ADQ_FOLLOW_RECORD_TRANSFER_BUFFER;

                    if (trigger_source == ADQ_EVENT_SOURCE_SOFTWARE)
                    {
                        using_software_trigger = true;
                    }
                }
                else
                {
                    absp_logger_error->error("{} Channel out of range, ignoring it (got: {});", log_name, id);
                }
            }
            else
            {
                absp_logger_error->error("{} Invalid channel number in \"id\" entry, ignoring it;", log_name);
            }
        }
    }

    // -------------------------------------------------------------------------
    //  Reading the event sources configuration
    // -------------------------------------------------------------------------
    json_t *json_event_source = json_object_get(config, "event_source");

    if (json_event_source != NULL && json_is_object(json_event_source))
    {
        // We will not force the existance of the event_source member as it is probably not used by most users
        absp_logger_console->info("{} Reading event_source configuration;", log_name);

        json_t *json_level_matrix = json_object_get(json_event_source, "level_matrix");
        if (json_level_matrix != NULL && json_is_object(json_level_matrix))
        {
            absp_logger_console->info("{} Reading event_source.level_matrix configuration;", log_name);

            json_t *json_channels = json_object_get(json_level_matrix, "channels");
            if (json_channels == NULL || !json_is_array(json_channels))
            {
                json_channels = json_array();
                json_object_set_nocheck(json_level_matrix, "channels", json_channels);
            }

            absp_logger_console->info("{} Reading event_source.level_matrix.channels configuration;", log_name);

            size_t index;
            json_t *value;

            json_array_foreach(json_channels, index, value)
            {
                json_t *json_id = json_object_get(value, "id");

                if (json_id != NULL && json_is_integer(json_id))
                {
                    const int id = json_integer_value(json_id);

                    absp_logger_console->info("{} Found channel: {};", log_name, id);

                    const bool enabled = json_is_true(json_object_get(value, "enable"));

                    absp_logger_console->info("{} Channel is {};", log_name, (enabled ? "disabled" : "enabled"));

                    json_object_set_nocheck(value, "enable", json_boolean(enabled));

                    const char *cstr_slope = json_string_value(json_object_get(value, "slope"));
                    const std::string str_slope = (cstr_slope) ? std::string(cstr_slope) : std::string();

                    // Looking for the settings in the description map
                    const auto tsl_result = map_utilities::find_item(ADQ_descriptions::slope, str_slope);

                    enum ADQEdge slope = ADQ_EDGE_RISING;

                    if (tsl_result != ADQ_descriptions::slope.end() && str_slope.length() > 0)
                    {
                        slope = tsl_result->first;
                    }
                    else
                    {
                        slope = ADQ_EDGE_RISING;

                        absp_logger_error->error("{} Invalid trigger slope, got: {};", log_name, str_slope);
                    }

                    absp_logger_console->info("{} Trigger slope: {};", log_name, ADQ_descriptions::slope.at(slope));
                    json_object_set_nocheck(value, "slope", json_string(ADQ_descriptions::slope.at(slope).c_str()));

                    adq_parameters.event_source.level_matrix.channel[id].enabled = enabled ? 1 : 0;
                    adq_parameters.event_source.level_matrix.channel[id].edge = slope;
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    //  Reading the native ADQ36 configuration
    // -------------------------------------------------------------------------
    native_config.clear();

    json_t *json_native_config = json_object_get(config, "native");

    if (json_is_object(json_native_config))
    {
        absp_logger_error->warn("{} Using the \"native\" entry in the configuration;", log_name);

        char *native_config_buffer = json_dumps(json_native_config, JSON_COMPACT);

        if (!native_config_buffer)
        {
            absp_logger_error->error("{} Unable to allocate native config buffer;", log_name);
        }
        else
        {
            size_t total_size = strlen(native_config_buffer);

            int validation = ADQ_ValidateParametersString(adq_cu_ptr, adq_num,
                                                          native_config_buffer, total_size);

            if (validation < 0)
            {
                absp_logger_error->error("{} The native config is not valid;", log_name);
            }
            else
            {
                native_config = native_config_buffer;
            }

            free(native_config_buffer);
        }
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::SpecificCommand(json_t *json_command)
{
    const std::string log_name = GetName() + " " + GetModel() + "::SpecificCommand()";

    const char *cstr_command = json_string_value(json_object_get(json_command, "command"));
    const std::string command = (cstr_command) ? std::string(cstr_command) : std::string();

    absp_logger_console->info("{} Specific command: {};", log_name, command);

    if (command == std::string("set_parameters"))
    {
        // ---------------------------------------------------------------------
        //  Set parameters
        // ---------------------------------------------------------------------
        const json_t *parameters = json_object_get(json_command, "parameters");

        absp_logger_console->info("{} Setting parameters", log_name);

        return SetParametersJSON(parameters);
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

std::string ABCD::ADQ36_FWDAQ::GetStatusString(enum ADQStatusId status_id)
{
    const std::string log_name = GetName() + " " + GetModel() + "::GetStatusString()";

    absp_logger_console->info("{} Getting status of id: {};", log_name, (int)status_id);

    char status[ADQAPI_JSON_BUFFER_SIZE];

    const int result = ADQ_GetStatusString(adq_cu_ptr, adq_num,
                                           status_id, status, ADQAPI_JSON_BUFFER_SIZE, 1);

    if (result < 0)
    {
        // Making sure that the string has length zero
        status[0] = '\0';
    }

    return std::string(status);
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::GetParameters(enum ADQParameterId parameter_id, void *const parameters)
{
    const std::string log_name = GetName() + " " + GetModel() + "::GetParameters()";

    absp_logger_console->info("{} Reading parameters of id: {};", log_name, (int)parameter_id);

    const int result = ADQ_GetParameters(adq_cu_ptr, adq_num,
                                         parameter_id, parameters);

    if (result < 0)
    {
        absp_logger_error->error("{} Unable to get parameters with id: {};", log_name, (int)parameter_id);

        return DIGITIZER_FAILURE;
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::SetParameters(void *const parameters)
{
    const std::string log_name = GetName() + " " + GetModel() + "::SetParameters()";

    absp_logger_console->info("{} Setting parameters;", log_name);

    const int result = ADQ_SetParameters(adq_cu_ptr, adq_num,
                                         parameters);

    if (result < 0)
    {
        char error_string[512];
        ADQControlUnit_GetLastFailedDeviceErrorWithText(adq_cu_ptr, error_string);
        absp_logger_error->error("{} Unable to set parameters (code: {}): {};", log_name, result, error_string);

        return DIGITIZER_FAILURE;
    }
    else
    {
        return DIGITIZER_SUCCESS;
    }
}

//==============================================================================

std::string ABCD::ADQ36_FWDAQ::GetParametersString(enum ADQParameterId parameter_id)
{
    const std::string log_name = GetName() + " " + GetModel() + "::GetParametersString()";

    absp_logger_console->info("{} Reading parameters of id: {};", log_name, (int)parameter_id);

    char parameters[ADQAPI_JSON_BUFFER_SIZE];

    const int result = ADQ_GetParametersString(adq_cu_ptr, adq_num,
                                               parameter_id, parameters, ADQAPI_JSON_BUFFER_SIZE, 1);

    if (result < 0)
    {
        // Making sure that the string has length zero
        parameters[0] = '\0';
    }

    return std::string(parameters);
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::SetParametersString(const std::string parameters)
{
    const std::string log_name = GetName() + " " + GetModel() + "::SetParametersString()";

    absp_logger_console->info("{} Setting parameters: {};", log_name, parameters);

    const int result = ADQ_SetParametersString(adq_cu_ptr, adq_num,
                                               parameters.c_str(), parameters.length());

    if (result < 0)
    {
        char error_string[512];
        ADQControlUnit_GetLastFailedDeviceErrorWithText(adq_cu_ptr, error_string);
        absp_logger_error->error("{} Unable to set parameters (code: {}): {};", log_name, result, error_string);

        return DIGITIZER_FAILURE;
    }
    else
    {
        return DIGITIZER_SUCCESS;
    }
}

//==============================================================================

json_t *ABCD::ADQ36_FWDAQ::GetParametersJSON(enum ADQParameterId parameter_id)
{
    const std::string log_name = GetName() + " " + GetModel() + "::GetParametersJSON()";

    absp_logger_console->info("{} Reading parameters of id: {};", log_name, (int)parameter_id);

    char parameters[ADQAPI_JSON_BUFFER_SIZE];

    const int result = ADQ_GetParametersString(adq_cu_ptr, adq_num,
                                               parameter_id, parameters, ADQAPI_JSON_BUFFER_SIZE, 1);

    if (result < 0)
    {
        // Making sure that the string has length zero
        parameters[0] = '\0';
    }

    json_error_t error;
    json_t *parameters_json = json_loads(parameters, 0, &error);

    if (!parameters_json)
    {
        absp_logger_error->error("{} Parse error while reading parameters string: {} (source: {}, line: {}, column: {}, position: {});", log_name, error.text, error.source, error.line, error.column, error.position);

        return NULL;
    }
    else
    {
        return parameters_json;
    }
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::SetParametersJSON(const json_t *parameters)
{
    const std::string log_name = GetName() + " " + GetModel() + "::SetParametersJSON()";

    absp_logger_console->info("{} Setting parameters;", log_name);

    char *parameters_buffer = json_dumps(parameters, JSON_COMPACT);

    if (!parameters_buffer)
    {
        absp_logger_error->error("{} Unable to create the JSON parameters buffer;", log_name);

        return DIGITIZER_FAILURE;
    }

    const int result = ADQ_SetParametersString(adq_cu_ptr, adq_num,
                                               parameters_buffer, strlen(parameters_buffer));

    free(parameters_buffer);

    if (result < 0)
    {
        char error_string[512];
        ADQControlUnit_GetLastFailedDeviceErrorWithText(adq_cu_ptr, error_string);
        absp_logger_error->error("{} Unable to set parameters (code: {}): {};", log_name, result, error_string);

        return DIGITIZER_FAILURE;
    }
    else
    {
        return DIGITIZER_SUCCESS;
    }
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::CustomFirmwareSetMode(unsigned int mode)
{
    const std::string log_name = GetName() + " " + GetModel() + "::CustomFirmwareSetMode()";

    if (mode > 1)
    {
        absp_logger_error->error("{} Custom firmware: Mode can only be 0 or 1  (got: {});", log_name, mode);

        return DIGITIZER_FAILURE;
    }

    absp_logger_console->info("{} Setting mode to: {};", log_name, (mode == 1 ? "forward to sync" : "daisy chain only"));

    // Identifies which user-logic core we want to write to
    const int USER_LOGIC_BLOCK_TARGET = 2;

    // The register number that sets the working mode
    const uint32_t REGISTER_NUMBER_MODE = 1;

    const uint32_t CUSTOM_FIRMWARE_MASK = 0;

    // This is the value returned from the WriteUserRegister() call.
    // It should contain the same value that was written to the register.
    uint32_t return_value;

    CHECKNEGATIVE(ADQ_WriteUserRegister(adq_cu_ptr, adq_num,
                                        USER_LOGIC_BLOCK_TARGET,
                                        REGISTER_NUMBER_MODE,
                                        CUSTOM_FIRMWARE_MASK,
                                        static_cast<uint32_t>(mode),
                                        &return_value));

    if (return_value != mode)
    {
        absp_logger_error->error("{} Custom firmware: Unable to set the mode {};", log_name, mode);

        return DIGITIZER_FAILURE;
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::CustomFirmwareSetPulseLength(unsigned int pulse_length)
{
    const std::string log_name = GetName() + " " + GetModel() + "::CustomFirmwareSetPulseLength()";

    absp_logger_console->info("{} Pulse length: {};", log_name, pulse_length);

    // Identifies which user-logic core we want to write to
    const int USER_LOGIC_BLOCK_TARGET = 2;

    // The register number that sets the pulse length
    const uint32_t REGISTER_NUMBER_PULSE_LENGTH = 2;

    const uint32_t CUSTOM_FIRMWARE_MASK = 0;

    // This is the value returned from the WriteUserRegister() call.
    // It should contain the same value that was written to the register.
    uint32_t return_value;

    CHECKNEGATIVE(ADQ_WriteUserRegister(adq_cu_ptr, adq_num,
                                        USER_LOGIC_BLOCK_TARGET,
                                        REGISTER_NUMBER_PULSE_LENGTH,
                                        CUSTOM_FIRMWARE_MASK,
                                        static_cast<uint32_t>(pulse_length),
                                        &return_value));

    if (return_value != pulse_length)
    {
        absp_logger_error->error("{} Custom firmware: Unable to set the pulse length {};", log_name, pulse_length);

        return DIGITIZER_FAILURE;
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::CustomFirmwareEnable(bool enable)
{
    const std::string log_name = GetName() + " " + GetModel() + "::CustomFirmwareEnable()";

    absp_logger_console->info("{} Enabling custom firmware functionality: {};", log_name, (enable ? "true" : "false"));

    // Identifies which user-logic core we want to write to
    const int USER_LOGIC_BLOCK_TARGET = 2;

    // The register number that enables the functioning of the custom firmware
    const uint32_t REGISTER_NUMBER_ENABLE = 3;

    const uint32_t CUSTOM_FIRMWARE_MASK = 0;

    // This is the value returned from the WriteUserRegister() call.
    // It should contain the same value that was written to the register.
    uint32_t return_value;

    CHECKNEGATIVE(ADQ_WriteUserRegister(adq_cu_ptr, adq_num,
                                        USER_LOGIC_BLOCK_TARGET,
                                        REGISTER_NUMBER_ENABLE,
                                        CUSTOM_FIRMWARE_MASK,
                                        enable ? 1 : 0,
                                        &return_value));

    if (return_value != (enable ? 1 : 0))
    {
        absp_logger_error->error("{} Custom firmware: Unable to enable the custom firmware functioning;", log_name);

        return DIGITIZER_FAILURE;
    }

    return DIGITIZER_SUCCESS;
}
