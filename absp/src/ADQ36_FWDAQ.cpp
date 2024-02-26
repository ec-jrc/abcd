#include <cstdlib>
#include <cinttypes>

#include <fstream>
#include <iostream>
#include <stdexcept>

#include <thread>
#include <chrono>

extern "C" {
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

#define BUFFER_SIZE 32
// The dump of the default configuration has 47130 characters
#define ADQAPI_JSON_BUFFER_SIZE 65536

ABCD::ADQ36_FWDAQ::ADQ36_FWDAQ(void* adq, int num, int Verbosity) : ABCD::Digitizer(Verbosity),
                                                                  adq_cu_ptr(adq),
                                                                  adq_num(num)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ADQ36_FWDAQ() ";
        std::cout << std::endl;
    }

    SetModel("ADQ36_FWDAQ");

    SetEnabled(false);

    timestamp_bit_shift = 0;

    // This is reset only at the class creation because the timestamp seems to
    // be growing continuously even after starts or stops.
    timestamp_last = 0;
    timestamp_offset = 0;
    timestamp_overflows = 0;
}

//==============================================================================

ABCD::ADQ36_FWDAQ::~ADQ36_FWDAQ() {
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::Initialize()
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::Initialize() ";
        std::cout << std::endl;
    }

    // Initialize the variable holding the configuration parameters
    if (ADQ_InitializeParameters(adq_cu_ptr, adq_num,
                                 ADQ_PARAMETER_ID_TOP, &adq_parameters) != sizeof(adq_parameters))
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::Initialize() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << " Failed to initialize digitizer parameters; ";
        std::cout << std::endl;

        return DIGITIZER_FAILURE;
    }

    SetName(adq_parameters.constant.serial_number);

    SetChannelsNumber(adq_parameters.constant.nof_channels);

    if (GetVerbosity() > 0)
    {
        std::string output_string(ADQAPI_JSON_BUFFER_SIZE, '\0');

        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::Initialize() ";
        std::cout << "Initialized board; ";
        std::cout << std::endl;
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::Initialize() ";
        std::cout << "Card name (serial number): " << GetName() << "; ";
        std::cout << "Product name: " << adq_parameters.constant.product_name << "; ";
        std::cout << "Card option: " << adq_parameters.constant.product_options << "; ";
        std::cout << "DNA: " << adq_parameters.constant.dna << "; ";
        std::cout << std::endl;

        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::Initialize() ";
        std::cout << "ADQAPI Revision: " << ADQAPI_GetRevision() << "; ";
        std::cout << std::endl;

        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::Initialize() ";
        std::cout << "Channels number: " << GetChannelsNumber() << "; ";
        std::cout << std::endl;

        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::Initialize() ";
        std::cout << "Firmware type: " << adq_parameters.constant.firmware.type << "; ";
        std::cout << "name: " << adq_parameters.constant.firmware.name << "; ";
        std::cout << "revision: " << adq_parameters.constant.firmware.revision << "; ";
        std::cout << "customization: " << adq_parameters.constant.firmware.customization << "; ";
        std::cout << "part number: " << adq_parameters.constant.firmware.part_number << "; ";
        std::cout << std::endl;

        CHECKNEGATIVE(ADQ_GetStatusString(adq_cu_ptr, adq_num,
                                          ADQ_STATUS_ID_TEMPERATURE,
                                          const_cast<char*>(output_string.c_str()),
                                          ADQAPI_JSON_BUFFER_SIZE, 1));

        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::Initialize() ";
        std::cout << "Temperature JSON status: " << output_string << "; ";
        std::cout << std::endl;

        CHECKNEGATIVE(ADQ_GetStatusString(adq_cu_ptr, adq_num,
                                          ADQ_STATUS_ID_ACQUISITION,
                                          const_cast<char*>(output_string.c_str()),
                                          ADQAPI_JSON_BUFFER_SIZE, 1));

        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::Initialize() ";
        std::cout << "Acquisition JSON status: " << output_string << "; ";
        std::cout << std::endl;

        if (GetVerbosity() > 1) {
            std::string file_name("ADQ36_");
            file_name += GetName();
            file_name += "_default-config_";
            file_name += time_buffer;
            file_name += ".json";

            std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::Initialize() ";
            std::cout << "Saving digitizer parameters to file: " << file_name << "; ";
            std::cout << std::endl;
            CHECKNEGATIVE(ADQ_InitializeParametersFilename(adq_cu_ptr, adq_num,
                                                           ADQ_PARAMETER_ID_TOP,
                                                           file_name.c_str(),
                                                           1));
        }

    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::Configure()
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::Configure() ";
        std::cout << "Configuring board; ";
        std::cout << std::endl;
    }

    if (!IsEnabled()) {
        return DIGITIZER_SUCCESS; // if card is OFF return immediately
    }

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::Configure() ";
        std::cout << "Setting parameters; ";
        std::cout << std::endl;
    }

    const int sp_result = ADQ_SetParameters(adq_cu_ptr, adq_num,
                                            reinterpret_cast<void*>(&adq_parameters));

    if (sp_result < 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::Configure() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << " Unable to set parameters; ";
        std::cout << std::endl;

        return DIGITIZER_FAILURE;
    }

    // -------------------------------------------------------------------------
    //  Custom JRC-Geel firmware configuration
    // -------------------------------------------------------------------------
    // We write the registers only if the custom-firmware was enabled, otherwise
    // we risk to write over registers of standard firmwares that we are not
    // supposed to.
    if (custom_firmware_enabled) {
        CustomFirmwareSetMode(custom_firmware_mode);
        CustomFirmwareSetPulseLength(custom_firmware_pulse_length);
        CustomFirmwareEnable(custom_firmware_enabled);
    }

    // -------------------------------------------------------------------------
    //  Managing the native JSON configuration of the digitizer
    // -------------------------------------------------------------------------
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);

        std::string file_name("ADQ36_");
        file_name += GetName();
        file_name += "_post-config_";
        file_name += time_buffer;
        file_name += ".json";

        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::Initialize() ";
        std::cout << "Saving digitizer parameters after configuration to file: " << file_name << "; ";
        std::cout << std::endl;
        CHECKNEGATIVE(ADQ_GetParametersFilename(adq_cu_ptr, adq_num,
                                                ADQ_PARAMETER_ID_TOP,
                                                file_name.c_str(),
                                                1));
    }

    if (!native_config.empty()) {
        CHECKNEGATIVE(ADQ_SetParametersString(adq_cu_ptr, adq_num,
                                              native_config.c_str(), native_config.length()));

        if (GetVerbosity() > 1)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);

            std::string file_name("ADQ36_");
            file_name += GetName();
            file_name += "_post-native-config_";
            file_name += time_buffer;
            file_name += ".json";

            std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::Initialize() ";
            std::cout << "Saving digitizer parameters after configuration to file: " << file_name << "; ";
            std::cout << std::endl;
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
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::SetChannelsNumber() ";
        std::cout << "Setting channels number to: " << n << "; ";
        std::cout << std::endl;
    }

    Digitizer::SetChannelsNumber(n);
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::StartAcquisition()
{
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::StartAcquisition() ";
        std::cout << "Starting acquisition; ";
        std::cout << std::endl;
    }

    //last_buffer_ready = std::chrono::system_clock::now();


    const int retval = ADQ_StartDataAcquisition(adq_cu_ptr, adq_num);

    if (retval != ADQ_EOK) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::StartAcquisition() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Unable to start acquisition ";
        std::cout << " (code: " << WRITE_YELLOW << retval << WRITE_NC << "); ";
        std::cout << std::endl;
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::StartAcquisition() ";
        std::cout << WRITE_RED << ADQ_descriptions::error.at(retval) << WRITE_NC << "; ";
        std::cout << std::endl;

        return DIGITIZER_FAILURE;
    }

    if (using_software_trigger) {
        // We need to set it at least to 1 to force at least one trigger
        int max_records_number = 1;

        for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++) {
            if (adq_parameters.acquisition.channel[channel].nof_records > max_records_number) {
                max_records_number = adq_parameters.acquisition.channel[channel].nof_records;
            }
        }

        for (int i = 0; i < max_records_number; i++) {
            ForceSoftwareTrigger();
        }
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::RearmTrigger()
{
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::RearmTrigger() ";
        std::cout << "Rearming trigger; ";
        std::cout << std::endl;
    }

    if (using_software_trigger) {
        if (GetVerbosity() > 1)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::RearmTrigger() ";
            std::cout << "Forcing software trigger; ";
            std::cout << std::endl;
        }

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
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::GetWaveformsFromCard() ";
        std::cout << "Getting waveforms from card; ";
        std::cout << std::endl;
    }

    int64_t available_records = ADQ_EAGAIN;

    do {
        int available_channel = ADQ_ANY_CHANNEL;
        struct ADQGen4RecordArray *ADQ_records_array = NULL;
        struct ADQDataReadoutStatus ADQ_status;

        // Using a zero here should make the function return immediately
        const int timeout = 0;

        available_records = ADQ_WaitForRecordBuffer(adq_cu_ptr, adq_num,
                                                              &available_channel,
                                                              reinterpret_cast<void**>(&ADQ_records_array),
                                                              timeout,
                                                              &ADQ_status);

        if (available_records == 0) {
            // This is a status only record, I am not sure how to handle it
            if (GetVerbosity() > 1)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::GetWaveformsFromCard() ";
                std::cout << "Status only record: flags: ";
                printf("%08" PRIu32 "", ADQ_status.flags);
                std::cout << "; ";
                std::cout << "Available channel: " << available_channel << "; ";
                std::cout << std::endl;
            }
        } else if (available_records == ADQ_EAGAIN) {
            // This is a safe timeout, it means that data is not yet
            // available
            if (GetVerbosity() > 1)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::GetWaveformsFromCard() ";
                std::cout << "Timeout; ";
                std::cout << std::endl;
            }
        } else if (available_records < 0 && available_records != ADQ_EAGAIN) {
            // This is an error!
            // The record should not have been allocated and the docs
            // suggest to stop the acquisition to understand the error value
            if (GetVerbosity() > 1)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::GetWaveformsFromCard() ";
                std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Error code: " << (long)available_records << "; ";
                std::cout << std::endl;
            }
            return DIGITIZER_FAILURE;

        } else if (available_records > 0) {
            if (GetVerbosity() > 1)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::GetWaveformsFromCard() ";
                std::cout << "Available data; ";
                std::cout << "Available records: " << (long)available_records << "; ";
                std::cout << std::endl;
            }

            for (int64_t record_index = 0; record_index < available_records; record_index++) {

                // At this point the ADQ_record variable should be ready...
                const uint8_t channel = ADQ_records_array->record[record_index]->header->channel;
                const uint32_t record_number = ADQ_records_array->record[record_index]->header->record_number;
                const uint32_t samples_per_record = ADQ_records_array->record[record_index]->header->record_length;
                const uint64_t timestamp = ADQ_records_array->record[record_index]->header->timestamp;

                if (GetVerbosity() > 1)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::GetWaveformsFromCard() ";
                    std::cout << "Available channel: " << available_channel << ", " << (unsigned int)channel << "; ";
                    std::cout << "Collected record number: " << (unsigned long)record_number << "; ";
                    std::cout << "Record length: " << (unsigned long)samples_per_record << "; ";
                    std::cout << std::endl;
                }

                // If we see a jump backward in timestamp bigger than half the timestamp
                // range we assume that there was an overflow in the timestamp counter.
                // A smaller jump could mean that the records in the buffer are not
                // sorted according to the timestamp, that would make sense but we have
                // not verified it.
                const int64_t timestamp_negative_difference = timestamp_last - timestamp;

                if (timestamp_negative_difference > ADQ36_FWDAQ_TIMESTAMP_THRESHOLD) {
                    if (GetVerbosity() > 0)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::GetWaveformsFromCard() ";
                        std::cout << WRITE_YELLOW << "WARNING" << WRITE_NC << ": Detected timestamp overflow; ";
                        std::cout << "Overflows: " << timestamp_overflows << "; ";
                        std::cout << "Negative difference: " << (long long)timestamp_negative_difference << "; ";
                        std::cout << std::endl;
                    }

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

                uint16_t * const samples = waveform_samples_get(&this_waveform);

                if (ADQ_records_array->record[record_index]->header->data_format == ADQ_DATA_FORMAT_INT16) {
                    const int16_t *data_buffer_16bits = reinterpret_cast<int16_t*>(ADQ_records_array->record[record_index]->data);

                    for (unsigned int sample_index = 0; sample_index < samples_per_record; sample_index++) {
                        const int16_t value = data_buffer_16bits[sample_index];

                        // We add an offset to match it to the rest of ABCD
                        // WARNING: The 12 bit data is aligned to the MSB
                        // Signal processing modules in the data path utilize
                        // the full bit range for calculations. Therefore,
                        // ignoring the least significant bits in the output can
                        // result in a loss of accuracy.
                        samples[sample_index] = ((value) + (1 << 15));
                    }
                } else if (ADQ_records_array->record[record_index]->header->data_format == ADQ_DATA_FORMAT_INT32) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::GetWaveformsFromCard() ";
                    std::cout << WRITE_YELLOW << "WARNING" << WRITE_NC << ": Data format is set to 32 bits; ";
                    std::cout << std::endl;

                    const int32_t *data_buffer_32bits = reinterpret_cast<int32_t*>(ADQ_records_array->record[record_index]->data);

                    for (unsigned int sample_index = 0; sample_index < samples_per_record; sample_index++) {
                        const int16_t value = data_buffer_32bits[sample_index];

                        // We add an offset to match it to the rest of ABCD
                        samples[sample_index] = ((value) + (1 << 15));
                    }
                } else {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::GetWaveformsFromCard() ";
                    std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Unexpected data format; ";
                    std::cout << std::endl;
                }

                waveforms.push_back(this_waveform);
            }

            // ReturnRecordBuffer() signals to the API that we are done with the
            // record memory and it can be used again for another record.
            CHECKNEGATIVE(ADQ_ReturnRecordBuffer(adq_cu_ptr, adq_num,
                                                 available_channel,
                                                 ADQ_records_array));
        }

    } while (available_records >= 0);

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::GetWaveformsFromCard() ";
        std::cout << "Converted all samples; ";
        std::cout << "Timestamp overflows: " << timestamp_overflows << "; ";
        std::cout << std::endl;
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::StopAcquisition()
{
    {
        const int retval = ADQ_StopDataAcquisition(adq_cu_ptr, adq_num);

        if (retval != ADQ_EOK && retval != ADQ_EINTERRUPTED) {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::StopAcquisition() ";
            std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Stop acquisition error ";
            std::cout << " (code: " << WRITE_YELLOW << retval << WRITE_NC << "); ";
            std::cout << std::endl;
            std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::StopAcquisition() ";
            std::cout << WRITE_RED << ADQ_descriptions::error.at(retval) << WRITE_NC << "; ";
            std::cout << std::endl;

            return DIGITIZER_FAILURE;
        }
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::ForceSoftwareTrigger()
{
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ForceSoftwareTrigger() ";
        std::cout << "Forcing a software trigger; ";
        std::cout << std::endl;
    }

    CHECKNEGATIVE(ADQ_SWTrig(adq_cu_ptr, adq_num));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::ReadConfig(json_t *config)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
        std::cout << "Reading configration JSON; ";
        std::cout << std::endl;
    }

    const bool enable = json_is_true(json_object_get(config, "enable"));
    SetEnabled(enable);

    // Set again the settings that was just read, so in the case it was not present
    // the configuration becomes correct
    json_object_set_nocheck(config, "enable", json_boolean(enable));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
        std::cout << "Card is " << (enable ? "enabled" : "disabled") << "; ";
        std::cout << std::endl;
    }

    // -------------------------------------------------------------------------
    //  Reading the ABCD-style configuration
    // -------------------------------------------------------------------------
    using_software_trigger = false;

    // -------------------------------------------------------------------------
    //  Custom JRC-Geel firmware configuration
    // -------------------------------------------------------------------------
    json_t *json_custom_firmware = json_object_get(config, "JRC_custom_firmware");

    if (json_custom_firmware != NULL && json_is_object(json_custom_firmware)) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
        std::cout << WRITE_YELLOW << "WARNING" << WRITE_NC << ": JRC-Geel custom firmware settings detected";
        std::cout << std::endl;

        custom_firmware_enabled = json_is_true(json_object_get(json_custom_firmware, "enable"));

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
            std::cout << "Custom firmware settings are " << (custom_firmware_enabled ? "enabled" : "disabled") << "; ";
            std::cout << std::endl;
        }

        if (custom_firmware_enabled) {
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
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::Initialize() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << " Failed to initialize clock_system parameters; ";
        std::cout << std::endl;

        return DIGITIZER_FAILURE;
    }

    json_t *json_clock = json_object_get(config, "clock");

    if (!json_clock) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
        std::cout << WRITE_YELLOW << "WARNING" << WRITE_NC << ": Missing clock configuration";
        std::cout << std::endl;

        json_clock = json_object();

        json_object_set_new_nocheck(config, "clock", json_clock);
    }

    if (json_clock != NULL && json_is_object(json_clock))
    {
        const char *cstr_clock_source = json_string_value(json_object_get(json_clock, "source"));
        const std::string str_clock_source = (cstr_clock_source) ? std::string(cstr_clock_source) : std::string();

        // Looking for the settings in the description map
        const auto cs_result = map_utilities::find_item(ADQ_descriptions::ADQ36_clock_source, str_clock_source);

        if (cs_result != ADQ_descriptions::ADQ36_clock_source.end() && str_clock_source.length() > 0) {
            clock_system.reference_source = cs_result->first;
        } else {
            clock_system.reference_source = ADQ_REFERENCE_CLOCK_SOURCE_INTERNAL;

            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
            std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Wrong clock source";
            std::cout << std::endl;
        }

        // If the clock source is set to external, then the clock generator
        // should be set to external as well... maybe
        //if (clock_system.reference_source == ADQ_REFERENCE_CLOCK_SOURCE_INTERNAL) {
        //    clock_system.clock_generator = ADQ_CLOCK_GENERATOR_INTERNAL_PLL;
        //} else {
        //    clock_system.clock_generator = ADQ_CLOCK_GENERATOR_EXTERNAL_CLOCK;
        //}

        json_object_set_nocheck(json_clock, "source", json_string(ADQ_descriptions::ADQ36_clock_source.at(adq_parameters.constant.clock_system.reference_source).c_str()));

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
            std::cout << "Clock source: got: " << ADQ_descriptions::ADQ36_clock_source.at(adq_parameters.constant.clock_system.reference_source) << " (index: " << adq_parameters.constant.clock_system.reference_source << "); ";
            std::cout << std::endl;
        }

        clock_system.reference_frequency = json_number_value(json_object_get(json_clock, "reference_frequency"));

        clock_system.low_jitter_mode_enabled = 1;
    }

    timestamp_bit_shift = json_number_value(json_object_get(config, "timestamp_bit_shift"));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
        std::cout << "Timestamp bit shift: " << timestamp_bit_shift << " bits; ";
        std::cout << std::endl;
    }

    json_object_set_nocheck(config, "timestamp_bit_shift", json_integer(timestamp_bit_shift));

    const int Scs_result = ADQ_SetParameters(adq_cu_ptr, adq_num, reinterpret_cast<void*>(&clock_system));
    if (Scs_result < 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::Initialize() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << " Failed to initialize clock_system parameters; ";
        std::cout << std::endl;

        return DIGITIZER_FAILURE;
    }

    // -------------------------------------------------------------------------
    //  Reading the ports configuration
    // -------------------------------------------------------------------------

    json_t *json_ports = json_object_get(config, "ports");

    if (json_ports != NULL && json_is_array(json_ports))
    {
        size_t index;
        json_t *value;

        json_array_foreach(json_ports, index, value) {
            json_t *json_id = json_object_get(value, "id");

            if (json_id != NULL && json_is_string(json_id)) {
                const char *cstr_id = json_string_value(json_id);
                const std::string str_id = cstr_id ? std::string(cstr_id) : std::string("");

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                    std::cout << "Found port: " << str_id << "; ";
                    std::cout << std::endl;
                }

                // Looking for the settings in the description map
                const auto pi_result = map_utilities::find_item(ADQ_descriptions::ADQ36_port_ids, str_id);

                enum ADQParameterId id = ADQ_PARAMETER_ID_RESERVED;

                if (pi_result != ADQ_descriptions::ADQ36_port_ids.end() && str_id.length() > 0) {
                    id = pi_result->first;

                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                    std::cout << "Found matching port id; ";
                    std::cout << "Got: " << str_id << "; ";
                    std::cout << "index: " << static_cast<int>(id) << "; ";
                    std::cout << std::endl;
                } else {
                    id = ADQ_PARAMETER_ID_RESERVED;

                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                    std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Invalid port id; ";
                    std::cout << "Got: " << str_id << "; ";
                    std::cout << std::endl;
                }

                if (id != ADQ_PARAMETER_ID_RESERVED) {
                    const char *cstr_impedance = json_string_value(json_object_get(value, "input_impedance"));
                    const std::string str_impedance = (cstr_impedance) ? std::string(cstr_impedance) : std::string();

                    // Looking for the settings in the description map
                    const auto ii_result = map_utilities::find_item(ADQ_descriptions::input_impedance, str_impedance);

                    enum ADQImpedance impedance = ADQ_IMPEDANCE_HIGH;

                    if (ii_result != ADQ_descriptions::input_impedance.end() && str_impedance.length() > 0) {
                        impedance = ii_result->first;
                    } else {
                        impedance = ADQ_IMPEDANCE_HIGH;

                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Wrong input impedance, got: " << str_impedance << "; ";
                        std::cout << std::endl;
                    }

                    json_object_set_nocheck(value, "input_impedance",
                                            json_string(ADQ_descriptions::input_impedance.at(impedance).c_str()));

                    if (GetVerbosity() > 0)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                        std::cout << "Input impedance: got: " << ADQ_descriptions::input_impedance.at(impedance) << " (index: " << impedance << "); ";
                        std::cout << std::endl;
                    }

                    const char *cstr_direction = json_string_value(json_object_get(value, "direction"));
                    const std::string str_direction = (cstr_direction) ? std::string(cstr_direction) : std::string();

                    // Looking for the settings in the description map
                    const auto pd_result = map_utilities::find_item(ADQ_descriptions::pin_direction, str_direction);

                    enum ADQDirection direction = ADQ_DIRECTION_IN;

                    if (pd_result != ADQ_descriptions::pin_direction.end() && str_direction.length() > 0) {
                        direction = pd_result->first;
                    } else {
                        direction = ADQ_DIRECTION_IN;

                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Wrong pin direction, got: " << str_direction << "; ";
                        std::cout << std::endl;
                    }

                    json_object_set_nocheck(value, "direction",
                                            json_string(ADQ_descriptions::pin_direction.at(direction).c_str()));

                    if (GetVerbosity() > 0)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                        std::cout << "Pin direction: got: " << ADQ_descriptions::pin_direction.at(direction) << " (index: " << direction << "); ";
                        std::cout << std::endl;
                    }

                    const char *cstr_function = json_string_value(json_object_get(value, "function"));
                    const std::string str_function = (cstr_function) ? std::string(cstr_function) : std::string();

                    const auto pf_result = map_utilities::find_item(ADQ_descriptions::pin_function, str_function);

                    enum ADQFunction function = ADQ_FUNCTION_GPIO;

                    if (pf_result != ADQ_descriptions::pin_function.end() && str_function.length() > 0) {
                        function = pf_result->first;
                    } else {
                        function = ADQ_FUNCTION_GPIO;

                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Wrong pin function, got: " << str_function << "; ";
                        std::cout << std::endl;
                    }

                    json_object_set_nocheck(value, "function",
                                            json_string(ADQ_descriptions::pin_function.at(function).c_str()));

                    if (GetVerbosity() > 0)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                        std::cout << "Pin function: got: " << ADQ_descriptions::pin_function.at(function) << " (index: " << function << "); ";
                        std::cout << std::endl;
                    }

                    struct ADQPortParameters port_parameters;

                    int result = ADQ_InitializeParameters(adq_cu_ptr, adq_num, id, &port_parameters);
                    if (result != sizeof(port_parameters))
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::Initialize() ";
                        std::cout << WRITE_RED << "ERROR" << WRITE_NC << " Failed to initialize port parameters; ";
                        std::cout << std::endl;

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
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::Initialize() ";
                        std::cout << WRITE_RED << "ERROR" << WRITE_NC << " Failed to set port parameters; ";
                        std::cout << std::endl;

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
    for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++) {
        SetChannelEnabled(channel, false);
    }

    // Then reading the updated parameters from the board
    if (ADQ_GetParameters(adq_cu_ptr, adq_num,
                          ADQ_PARAMETER_ID_TOP, &adq_parameters) != sizeof(adq_parameters))
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::Initialize() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << " Failed to get digitizer parameters; ";
        std::cout << std::endl;

        return DIGITIZER_FAILURE;
    }

    // Finally reading the channels configuration
    json_t *json_channels = json_object_get(config, "channels");

    if (json_channels != NULL && json_is_array(json_channels))
    {
        size_t index;
        json_t *value;

        json_array_foreach(json_channels, index, value) {
            json_t *json_id = json_object_get(value, "id");

            if (json_id != NULL && json_is_integer(json_id)) {
                const int id = json_integer_value(json_id);

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                    std::cout << "Found channel: " << id << "; ";
                    std::cout << std::endl;
                }

                const bool enabled = json_is_true(json_object_get(value, "enable"));

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                    std::cout << "Channel is " << (enabled ? "enabled" : "disabled") << "; ";
                    std::cout << std::endl;
                }

                // -------------------------------------------------------------
                //  Analog front end settings (AFE)
                // -------------------------------------------------------------
                const double DC_offset = json_number_value(json_object_get(value, "DC_offset"));

                json_object_set_nocheck(value, "DC_offset", json_real(DC_offset));

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                    std::cout << "DC offset: " << DC_offset << " mV; ";
                    std::cout << std::endl;
                }

                // This is not available in the ADQ36 that we have
                //double input_range = json_number_value(json_object_get(value, "input_range"));

                //if (GetVerbosity() > 0)
                //{
                //    char time_buffer[BUFFER_SIZE];
                //    time_string(time_buffer, BUFFER_SIZE, NULL);
                //    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                //    std::cout << "Input range: " << input_range << " mVpp; ";
                //    std::cout << std::endl;
                //}

                //if (input_range <= 0) {
                //    input_range = default_input_range;
                //}

                //json_object_set_nocheck(value, "input_range", json_real(input_range));

                // -------------------------------------------------------------
                //  Signal processing parameters
                // -------------------------------------------------------------
                const bool DBS_enable = json_is_true(json_object_get(value, "DBS_enable"));

                json_object_set_nocheck(value, "DBS_enable", DBS_enable ? json_true() : json_false());

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                    std::cout << "DBS enable: " << (DBS_enable ? "true" : "false") << "; ";
                    std::cout << std::endl;
                }

                const int64_t DBS_level = json_number_value(json_object_get(value, "DBS_level"));

                json_object_set_nocheck(value, "DBS_level", json_integer(DBS_level));

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                    std::cout << "DBS level: " << (long long)DBS_level << "; ";
                    std::cout << std::endl;
                }

                const int raw_skip_factor = json_number_value(json_object_get(value, "skip_factor"));
                const int64_t skip_factor = (raw_skip_factor < 1) ? 1 : raw_skip_factor;

                json_object_set_nocheck(value, "skip_factor", json_integer(skip_factor));

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                    std::cout << "Skip factor: " << (long long)skip_factor << "; ";
                    std::cout << std::endl;
                }

                // -------------------------------------------------------------
                //  Event source parameters (triggering)
                // -------------------------------------------------------------
                const char *cstr_trigger_source = json_string_value(json_object_get(value, "trigger_source"));
                const std::string str_trigger_source = (cstr_trigger_source) ? std::string(cstr_trigger_source) : std::string();

                // Looking for the settings in the description map
                const auto tsr_result = map_utilities::find_item(ADQ_descriptions::event_source, str_trigger_source);

                enum ADQEventSource trigger_source = ADQ_EVENT_SOURCE_LEVEL;

                if (tsr_result != ADQ_descriptions::event_source.end() && str_trigger_source.length() > 0) {
                    trigger_source = tsr_result->first;
                } else {
                    trigger_source = ADQ_EVENT_SOURCE_LEVEL;

                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                    std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Wrong trigger source, got: " << str_trigger_source << "; ";
                    std::cout << std::endl;
                }

                json_object_set_nocheck(value, "trigger_source",
                                        json_string(ADQ_descriptions::event_source.at(trigger_source).c_str()));

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                    std::cout << "Trigger source: got: " << ADQ_descriptions::event_source.at(trigger_source) << " (index: " << trigger_source << "); ";
                    std::cout << std::endl;
                }

                // This level should be the relative to the baseline
                const int64_t trigger_level = json_number_value(json_object_get(value, "trigger_level"));

                json_object_set_nocheck(value, "trigger_level", json_integer(trigger_level));

                if (GetVerbosity() > 0) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                    std::cout << "Trigger level: " << (long long)trigger_level << "; ";
                    std::cout << std::endl;
                }

                const int64_t trigger_hysteresis = json_number_value(json_object_get(value, "trigger_hysteresis"));

                json_object_set_nocheck(value, "trigger_hysteresis", json_integer(trigger_hysteresis));

                if (GetVerbosity() > 0) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                    std::cout << "Trigger hysteresis: " << (long long)trigger_hysteresis << "; ";
                    std::cout << std::endl;
                }

                const char *cstr_trigger_slope = json_string_value(json_object_get(value, "trigger_slope"));
                const std::string str_trigger_slope = (cstr_trigger_slope) ? std::string(cstr_trigger_slope) : std::string();

                if (GetVerbosity() > 0) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                    std::cout << "Trigger slope: " << str_trigger_slope<< "; ";
                    std::cout << std::endl;
                }

                // Looking for the settings in the description map
                const auto tsl_result = map_utilities::find_item(ADQ_descriptions::slope, str_trigger_slope);

                enum ADQEdge trigger_slope = ADQ_EDGE_FALLING;

                if (tsl_result != ADQ_descriptions::slope.end() && str_trigger_slope.length() > 0) {
                    trigger_slope = tsl_result->first;

                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                    std::cout << "Found matching trigger slope; ";
                    std::cout << "Got: " << str_trigger_slope << "; ";
                    std::cout << "index: " << trigger_slope << "; ";
                    std::cout << std::endl;
                } else {
                    trigger_slope = ADQ_EDGE_FALLING;

                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                    std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Invalid trigger slope; ";
                    std::cout << "Got: " << str_trigger_slope << "; ";
                    std::cout << std::endl;
                }

                json_object_set_nocheck(value, "trigger_slope", json_string(ADQ_descriptions::slope.at(trigger_slope).c_str()));

                const int raw_pretrigger = json_number_value(json_object_get(value, "pretrigger"));

                const int64_t pretrigger = (raw_pretrigger - (raw_pretrigger % 8));

                json_object_set_nocheck(value, "pretrigger", json_integer(pretrigger));

                if (GetVerbosity() > 0) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                    std::cout << "Pretrigger: " << (long long)pretrigger << "; ";
                    std::cout << std::endl;
                }

                const unsigned int raw_scope_samples = json_number_value(json_object_get(value, "scope_samples"));
                const int64_t scope_samples = (raw_scope_samples < 32) ? 32 : raw_scope_samples;

                json_object_set_nocheck(value, "scope_samples", json_integer(scope_samples));

                if (GetVerbosity() > 0) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                    std::cout << "Scope samples: " << (long long)scope_samples << "; ";
                    std::cout << std::endl;
                }

                const unsigned int raw_records_per_buffer = json_number_value(json_object_get(value, "records_per_buffer"));

                const unsigned int records_per_buffer = (raw_records_per_buffer < 1) ? 1 : raw_records_per_buffer;

                json_object_set_nocheck(value, "records_per_buffer", json_integer(records_per_buffer));

                if (GetVerbosity() > 0) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                    std::cout << "Records per buffer: " << records_per_buffer << "; ";
                    std::cout << std::endl;
                }

                // This would limit the number of records read in an acquisition.
                // We hide this setting from the user because it can be confusing.
                // We will just use an infinite number of records in an acquisition.
                //const int raw_records_number = json_number_value(json_object_get(value, "records_number"));
                //const int records_number = (raw_records_number < 1) ? ADQ_INFINITE_NOF_RECORDS : raw_records_number;
                //json_object_set_nocheck(value, "records_number", json_integer(records_number));
                //if (GetVerbosity() > 0) {
                //    char time_buffer[BUFFER_SIZE];
                //    time_string(time_buffer, BUFFER_SIZE, NULL);
                //    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                //    std::cout << "Records number: " << records_number << "; ";
                //    std::cout << std::endl;
                //}

                // -------------------------------------------------------------------------
                //  Setting the channel configuration
                // -------------------------------------------------------------------------
                if (0 <= id && id < static_cast<int>(GetChannelsNumber())) {
                    SetChannelEnabled(id, enabled);

                    // Required to see ADC data
                    adq_parameters.test_pattern.channel[id].source = ADQ_TEST_PATTERN_SOURCE_DISABLE;

                    adq_parameters.afe.channel[id].dc_offset = DC_offset;
                    // This is not available in the ADQ36 that we have
                    //adq_parameters.afe.channel[id].input_range = input_range;

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
                    if (!enabled) {
                        adq_parameters.acquisition.channel[id].nof_records = 0;
                    }

                    adq_parameters.event_source.level.channel[id].level = trigger_level;
                    adq_parameters.event_source.level.channel[id].arm_hysteresis = trigger_hysteresis;

                    const int64_t bytes_per_sample = adq_parameters.acquisition.channel[id].bytes_per_sample;

                    adq_parameters.transfer.channel[id].record_size = bytes_per_sample * scope_samples;
                    adq_parameters.transfer.channel[id].record_buffer_size = bytes_per_sample * scope_samples * records_per_buffer;
                    adq_parameters.transfer.channel[id].metadata_buffer_size = sizeof(struct ADQGen4RecordHeader) * records_per_buffer;
                    adq_parameters.transfer.channel[id].record_length_infinite_enabled = 0;
                    adq_parameters.transfer.channel[id].metadata_enabled = 1;
                    // Using the maximum value here, we have no reason to use a smaller value
                    adq_parameters.transfer.channel[id].nof_buffers = ADQ_MAX_NOF_BUFFERS;

                    adq_parameters.readout.channel[id].nof_record_buffers_in_array = ADQ_FOLLOW_RECORD_TRANSFER_BUFFER;

                    if (trigger_source == ADQ_EVENT_SOURCE_SOFTWARE) {
                        using_software_trigger = true;
                    }
                } else {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                    std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Channel out of range, ignoring it; ";
                    std::cout << std::endl;
                }
            }
        }
    }

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
        std::cout << "Validating parameters with ADQ_ValidateParameters(); ";
        std::cout << std::endl;
    }

    const int vp_result = ADQ_ValidateParameters(adq_cu_ptr, adq_num,
                                                 reinterpret_cast<void*>(&adq_parameters));

    if (vp_result < 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << " Failure in parameters validation; ";
        std::cout << std::endl;

        return DIGITIZER_FAILURE;
    }

    // -------------------------------------------------------------------------
    //  Reading the native ADQ36 configuration
    // -------------------------------------------------------------------------
    native_config.clear();

    json_t *json_native_config = json_object_get(config, "native");

    if (json_is_object(json_native_config)) {
        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
            std::cout << WRITE_YELLOW << "WARNING" << WRITE_NC << ": Using the \"native\" entry in the configuration; ";
            std::cout << std::endl;
        }

        char *native_config_buffer = json_dumps(json_native_config, JSON_COMPACT);

        if (!native_config_buffer)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
            std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Unable to allocate native config buffer; ";
            std::cout << std::endl;
        }
        else
        {
            size_t total_size = strlen(native_config_buffer);

            int validation = ADQ_ValidateParametersString(adq_cu_ptr, adq_num,
                                                          native_config_buffer, total_size);

            if (validation < 0) {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::ReadConfig() ";
                std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": The native config is not valid; ";
                std::cout << std::endl;
            } else {
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
    const char *cstr_command = json_string_value(json_object_get(json_command, "command"));
    const std::string command = (cstr_command) ? std::string(cstr_command) : std::string();

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::SpecificCommand() ";
        std::cout << "Specific command: " << command << "; ";
        std::cout << std::endl;
    }

    if (command == std::string("set_parameters")) {
        // ---------------------------------------------------------------------
        //  Set parameters
        // ---------------------------------------------------------------------
        const json_t *parameters = json_object_get(json_command, "parameters");

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::SpecificCommand() ";
            std::cout << "Setting parameters; ";
            std::cout << std::endl;
        }

        return SetParametersJSON(parameters);
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

std::string ABCD::ADQ36_FWDAQ::GetStatusString(enum ADQStatusId status_id)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::GetStatusString() ";
        std::cout << "Reading status of id: " << (int)status_id << "; ";
        std::cout << std::endl;
    }

    char status[ADQAPI_JSON_BUFFER_SIZE];

    const int result = ADQ_GetStatusString(adq_cu_ptr, adq_num,
                                           status_id, status, ADQAPI_JSON_BUFFER_SIZE, 1);

    if (result < 0) {
        // Making sure that the string has length zero
        status[0] = '\0';
    }

    return std::string(status);
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::GetParameters(enum ADQParameterId parameter_id, void *const parameters)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::GetParameters() ";
        std::cout << "Reading parameters of id: " << (int)parameter_id << "; ";
        std::cout << std::endl;
    }

    const int result = ADQ_GetParameters(adq_cu_ptr, adq_num,
                                         parameter_id, parameters);

    if (result < 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::GetParameters() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << "Unable to get parameters with id: " << (int)parameter_id;
        std::cout << std::endl;

        return DIGITIZER_FAILURE;
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::SetParameters(void *const parameters)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::SetParameters() ";
        std::cout << "Setting parameters; ";
        std::cout << std::endl;
    }

    const int result = ADQ_SetParameters(adq_cu_ptr, adq_num,
                                         parameters);

    if (result < 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::GetParameters() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << "Unable to set parameters";
        std::cout << std::endl;

        return DIGITIZER_FAILURE;
    } else {
        return DIGITIZER_SUCCESS;
    }
}

//==============================================================================

std::string ABCD::ADQ36_FWDAQ::GetParametersString(enum ADQParameterId parameter_id)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::GetParametersString() ";
        std::cout << "Reading parameters of id: " << (int)parameter_id << "; ";
        std::cout << std::endl;
    }

    char parameters[ADQAPI_JSON_BUFFER_SIZE];

    const int result = ADQ_GetParametersString(adq_cu_ptr, adq_num,
                                               parameter_id, parameters, ADQAPI_JSON_BUFFER_SIZE, 1);

    if (result < 0) {
        // Making sure that the string has length zero
        parameters[0] = '\0';
    }

    return std::string(parameters);
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::SetParametersString(const std::string parameters)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::SetParametersString() ";
        std::cout << "Setting parameters; ";
        std::cout << std::endl;
    }

    const int result = ADQ_SetParametersString(adq_cu_ptr, adq_num,
                                               parameters.c_str(), parameters.length());

    if (result < 0) {
        return DIGITIZER_FAILURE;
    } else {
        return DIGITIZER_SUCCESS;
    }
}

//==============================================================================

json_t *ABCD::ADQ36_FWDAQ::GetParametersJSON(enum ADQParameterId parameter_id)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::GetParametersJSON() ";
        std::cout << "Reading parameters of id: " << (int)parameter_id << "; ";
        std::cout << std::endl;
    }

    char parameters[ADQAPI_JSON_BUFFER_SIZE];

    const int result = ADQ_GetParametersString(adq_cu_ptr, adq_num,
                                               parameter_id, parameters, ADQAPI_JSON_BUFFER_SIZE, 1);

    if (result < 0) {
        // Making sure that the string has length zero
        parameters[0] = '\0';
    }

    json_error_t error;
    json_t *parameters_json = json_loads(parameters, 0, &error);

    if (!parameters_json)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::GetParametersJSON() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << "Parse error while reading parameters string: ";
        std::cout << error.text;
        std::cout << " (source: " << error.source << ", line: " << error.line << ", column: " << error.column << ", position: " << error.position << ")";
        std::cout << std::endl;

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
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::SetParametersJSON() ";
        std::cout << "Setting parameters; ";
        std::cout << std::endl;
    }

    char *parameters_buffer = json_dumps(parameters, JSON_COMPACT);

    if (!parameters_buffer) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::SetParametersJSON() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << " Unable to create the JSON parameters buffer; ";
        std::cout << std::endl;

        return DIGITIZER_FAILURE;
    }

    const int result = ADQ_SetParametersString(adq_cu_ptr, adq_num,
                                               parameters_buffer, strlen(parameters_buffer));

    free(parameters_buffer);

    if (result < 0) {
        return DIGITIZER_FAILURE;
    } else {
        return DIGITIZER_SUCCESS;
    }
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::CustomFirmwareSetMode(uint32_t mode)
{
    if (mode > 1) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::CustomFirmwareSetMode() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << " Custom firmware: Mode can only be 0 or 1, got: " << mode << " ; ";
        std::cout << std::endl;

        return DIGITIZER_FAILURE;
    }

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::CustomFirmwareSetMode() ";
        std::cout << "Setting mode to: " << (mode == 1 ? "forward to sync" : "daisy chain only") << "; ";
        std::cout << std::endl;
    }

    // Identifies which user-logic core we want to write to
    const int USER_LOGIC_BLOCK_TARGET = 2;

    // The register number that sets the working mode
    const uint32_t REGISTER_NUMBER_MODE = 1;

    const uint32_t CUSTOM_FIRMWARE_MASK = 0;

    // This is the value returned from the WriteUserRegister() call.
    // It should contain the same value that was written to the register.
    uint32_t return_value;

    CHECKZERO(ADQ_WriteUserRegister(adq_cu_ptr, adq_num,
                                    USER_LOGIC_BLOCK_TARGET,
                                    REGISTER_NUMBER_MODE,
                                    CUSTOM_FIRMWARE_MASK,
                                    mode,
                                    &return_value));

    if (return_value != mode) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::CustomFirmwareSetMode() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << " Custom firmware: Unable to set the mode; ";
        std::cout << std::endl;

        return DIGITIZER_FAILURE;
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::CustomFirmwareSetPulseLength(uint32_t pulse_length)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::CustomFirmwareSetPulseLength() ";
        std::cout << "Pulse length to: " << pulse_length << "; ";
        std::cout << std::endl;
    }

    // Identifies which user-logic core we want to write to
    const int USER_LOGIC_BLOCK_TARGET = 2;

    // The register number that sets the pulse length
    const uint32_t REGISTER_NUMBER_PULSE_LENGTH = 2;

    const uint32_t CUSTOM_FIRMWARE_MASK = 0;

    // This is the value returned from the WriteUserRegister() call.
    // It should contain the same value that was written to the register.
    uint32_t return_value;

    CHECKZERO(ADQ_WriteUserRegister(adq_cu_ptr, adq_num,
                                    USER_LOGIC_BLOCK_TARGET,
                                    REGISTER_NUMBER_PULSE_LENGTH,
                                    CUSTOM_FIRMWARE_MASK,
                                    pulse_length,
                                    &return_value));

    if (return_value != pulse_length) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::CustomFirmwareSetPulseLength() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << " Custom firmware: Unable to set the pulse length; ";
        std::cout << std::endl;

        return DIGITIZER_FAILURE;
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ36_FWDAQ::CustomFirmwareEnable(bool enable)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::CustomFirmwareEnable() ";
        std::cout << "Enabling custom firmware functionality: " << (enable ? "true" : "false") << "; ";
        std::cout << std::endl;
    }

    // Identifies which user-logic core we want to write to
    const int USER_LOGIC_BLOCK_TARGET = 2;

    // The register number that enables the functioning of the custom firmware
    const uint32_t REGISTER_NUMBER_ENABLE = 3;

    const uint32_t CUSTOM_FIRMWARE_MASK = 0;

    // This is the value returned from the WriteUserRegister() call.
    // It should contain the same value that was written to the register.
    uint32_t return_value;

    CHECKZERO(ADQ_WriteUserRegister(adq_cu_ptr, adq_num,
                                    USER_LOGIC_BLOCK_TARGET,
                                    REGISTER_NUMBER_ENABLE,
                                    CUSTOM_FIRMWARE_MASK,
                                    enable ? 1 : 0,
                                    &return_value));

    if (return_value != (enable ? 1 : 0)) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ36_FWDAQ::CustomFirmwareEnable() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << " Custom firmware: Unable to enable the custom firmware functioning; ";
        std::cout << std::endl;

        return DIGITIZER_FAILURE;
    }

    return DIGITIZER_SUCCESS;
}
