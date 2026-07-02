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
}

#include "map_utilities.hpp"

#define LINUX
#include "ADQAPI.h"
#include "ADQ_descriptions.hpp"
#include "ADQ_utilities.hpp"
#include "ADQ14_FWDAQ.hpp"

// Defined in V
const double ABCD::ADQ14_FWDAQ::default_trig_ext_threshold = 0.5;
// Defined in mVpp
const float ABCD::ADQ14_FWDAQ::default_input_range = 1000;
// Defined in ADC samples
const int ABCD::ADQ14_FWDAQ::default_DC_offset = 0;
// Defined in ADC samples
const int ABCD::ADQ14_FWDAQ::default_DBS_target = 0;
// If left at zero the FWDAQ will use its default values
const int ABCD::ADQ14_FWDAQ::default_DBS_saturation_level_lower = 0;
const int ABCD::ADQ14_FWDAQ::default_DBS_saturation_level_upper = 0;

ABCD::ADQ14_FWDAQ::ADQ14_FWDAQ(void *adq, int num) : ABCD::Digitizer(),
                                                     adq_cu_ptr(adq),
                                                     adq_num(num)
{
    SetModel("ADQ14_FWDAQ");

    const std::string log_name = GetModel() + "::ADQ14_FWDAQ()";
    absp_logger_console->info("{}", log_name);

    SetEnabled(false);

    clock_source = ADQ_CLOCK_INT_INTREF;

    DBS_instances_number = 0;

    trigger_mode = ADQ_LEVEL_TRIGGER_MODE;
    trigger_slope = ADQ_TRIG_SLOPE_FALLING;
    trig_external_delay = 0;
    trigger_level = 0;

    records_number = 1024;
    samples_per_record = 2048;
    pretrigger = samples_per_record / 2;

    // This is reset only at the class creation because the timestamp seems to
    // be growing continuously even after starts or stops.
    timestamp_last = 0;
    timestamp_offset = 0;
    timestamp_overflows = 0;
    timestamp_bit_shift = 0;
}

//==============================================================================

ABCD::ADQ14_FWDAQ::~ADQ14_FWDAQ()
{
    const std::string log_name = GetName() + " " + GetModel() + "::~ADQ14_FWDAQ()";
    absp_logger_console->info("{}", log_name);
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::Initialize()
{
    std::string log_name = GetModel() + "::Initialize()";
    absp_logger_console->info("{}", log_name);

    // The vendor says that these should be used only for USB digitizers
    // CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_POWER_ON));
    // CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_COMMUNICATION));
    // CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_ADC_DATA));

    const char *board_name = ADQ_GetBoardSerialNumber(adq_cu_ptr, adq_num);

    if (!board_name)
    {
        absp_logger_error->error("{} Error in getting the serial number", log_name);

        SetName("");
    }
    else
    {
        SetName(board_name);
    }

    log_name = std::string(board_name) + std::string(" ") + log_name;

    const unsigned int number_of_channels = ADQ_GetNofChannels(adq_cu_ptr, adq_num);

    SetChannelsNumber(number_of_channels);

    unsigned int adc_cores = 0;
    CHECKZERO(ADQ_GetNofAdcCores(adq_cu_ptr, adq_num, &adc_cores));

    unsigned int dbs_inst = 0;
    CHECKZERO(ADQ_GetNofDBSInstances(adq_cu_ptr, adq_num, &dbs_inst));

    SetDBSInstancesNumber(dbs_inst);

    absp_logger_console->info("{} Initialized board", log_name);
    absp_logger_console->info("{} Card name (serial number): {}; Product name: {}; Card option: {}; Motherboard option: {};", log_name, GetName(), ADQ_GetBoardProductName(adq_cu_ptr, adq_num), ADQ_GetCardOption(adq_cu_ptr, adq_num), ADQ_GetADQDSPOption(adq_cu_ptr, adq_num));
    absp_logger_console->info("{} USB address: {}; PCIe address: {};", log_name, ADQ_GetUSBAddress(adq_cu_ptr, adq_num), ADQ_GetPCIeAddress(adq_cu_ptr, adq_num));
    absp_logger_console->info("{} ADQAPI revision: {}; ", log_name, ADQAPI_GetRevisionString());
    uint32_t *revision = ADQ_GetRevision(adq_cu_ptr, adq_num);
    std::string string_revision = "";

    for (int i = 0; i < 6; i++)
    {
        string_revision += std::to_string((unsigned int)revision[i]) + ", ";
    }
    absp_logger_console->info("{} ADQ14 Revision: {}", log_name, string_revision);
    absp_logger_console->info("{} Channels number: {}; ADC cores: {}; DBS instances: {}", log_name, GetChannelsNumber(), adc_cores, GetDBSInstancesNumber());
    absp_logger_console->info("{} Has adjustable input range: {}; Has adjustable offset: {}", log_name, (ADQ_HasAdjustableInputRange(adq_cu_ptr, adq_num) > 0 ? "true" : "false"), (ADQ_HasAdjustableBias(adq_cu_ptr, adq_num) > 0 ? "true" : "false"));
    // FIXME: Find out what trigger_num is to be used
    // std::cout << "Has adjustable external trigger threshold: " << (ADQ_HasVariableTrigThreshold(adq_cu_ptr, adq_num) > 0 ? "true" : "false") << "; ";

    for (auto &pair : ADQ_descriptions::ADQ14_temperatures)
    {
        const double temperature = ADQ_GetTemperature(adq_cu_ptr, adq_num, pair.first) / 256.0;

        absp_logger_console->info("{} {} temperature: {};", log_name, pair.second, temperature);
    }

    CHECKZERO(ADQ_SetOvervoltageProtection(adq_cu_ptr, adq_num, ADQ_OVERVOLTAGE_PROTECTION_ENABLE));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::Configure()
{
    const std::string log_name = GetName() + " " + GetModel() + "::Configure()";
    absp_logger_console->info("{} Configuring board;", log_name);

    if (!IsEnabled())
    {
        return DIGITIZER_SUCCESS; // if card is OFF return immediately
    }

    // -------------------------------------------------------------------------
    //  Clock settings
    // -------------------------------------------------------------------------
    absp_logger_console->info("{} Setting clock; clock_source: {};", log_name, ADQ_descriptions::clock_source.at(clock_source));
    CHECKZERO(ADQ_SetClockSource(adq_cu_ptr, adq_num, clock_source));

    absp_logger_console->info("{} Clock source from device: {};", log_name, ADQ_GetClockSource(adq_cu_ptr, adq_num));
    try
    {
        absp_logger_console->info("{} Clock source from device: {};", log_name, ADQ_descriptions::clock_source.at(ADQ_GetClockSource(adq_cu_ptr, adq_num)));
    }
    catch (...)
    {
    }

    absp_logger_console->info("{} Disabling test data;", log_name);

    // Disable test data and forward normal data to channels
    // in the example it is repeated at each channel, but to me it seems
    // unnecessary.
    CHECKZERO(ADQ_SetTestPatternMode(adq_cu_ptr, adq_num, 0));

    // -------------------------------------------------------------------------
    //  Channels settings
    // -------------------------------------------------------------------------
    channels_triggering_mask = 0;
    channels_acquisition_mask = 0;

    for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++)
    {
        absp_logger_console->info("{} Channel: {}; Enabled: {}; Triggering: {}", log_name, channel, (IsChannelEnabled(channel) ? "true" : "false"), (IsChannelTriggering(channel) ? "true" : "false"));

        if (IsChannelTriggering(channel))
        {
            channels_triggering_mask += (1 << channel);
        }
        if (IsChannelEnabled(channel))
        {
            channels_acquisition_mask += (1 << channel);
        }

        if (ADQ_HasAdjustableInputRange(adq_cu_ptr, adq_num) > 0)
        {
            absp_logger_console->info("{} Setting input range;");

            const float desired = desired_input_ranges[channel];
            float result = 0;

            CHECKZERO(ADQ_SetInputRange(adq_cu_ptr, adq_num, channel + 1, desired, &result));

            absp_logger_console->info("{} Input range, desired: {} mVpp, result: {} mVpp;", log_name, desired, result);
        }

        if (ADQ_HasAdjustableBias(adq_cu_ptr, adq_num) > 0)
        {
            const int DC_offset = DC_offsets[channel];
            // This is a physical DC offset added to the signal
            // while ADQ_SetGainAndOffset is a digital calculation
            absp_logger_console->info("{} Setting DC offset to: {};", log_name, DC_offset);

            CHECKZERO(ADQ_SetAdjustableBias(adq_cu_ptr, adq_num, channel + 1, DC_offset));

            int result = 0;

            CHECKZERO(ADQ_GetAdjustableBias(adq_cu_ptr, adq_num, channel + 1, &result));

            absp_logger_console->info("{} DC offset desired: {}; result: {}", log_name, DC_offset, result);
        }
    }

    // FIXME: This wait time, after the bias is adjusted, seems important for
    //        the new settings to take place. It might me, though, that the ABCD
    //        internal delays are enough for it.
    // if (ADQ_HasAdjustableBias(adq_cu_ptr, adq_num) > 0) {
    //    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    //}

    // -------------------------------------------------------------------------
    //  DBS settings
    // -------------------------------------------------------------------------

    for (unsigned int instance = 0; instance < GetDBSInstancesNumber(); instance++)
    {
        const bool DBS_disabled = DBS_disableds[instance];
        // We are assuming that the channels correspond to the DBS instances
        const int DC_offset = DC_offsets[instance];

        absp_logger_console->info("{} Setting DBS instance {} to {};", log_name, instance, (DBS_disabled ? "disabled" : "enabled"));

        // TODO: Enable these features
        CHECKZERO(ADQ_SetupDBS(adq_cu_ptr, adq_num, instance,
                               DBS_disabled,
                               DC_offset,
                               default_DBS_saturation_level_lower,
                               default_DBS_saturation_level_upper));
    }

    absp_logger_console->info("{} Channels acquisition mask: {}; Channels triggering mask: {};", log_name, (unsigned int)channels_acquisition_mask, channels_triggering_mask);

    // Only a set of possible masks are allowed, if the mask is wrong we use
    // the most conservative option of enabling all channels.
    if (!(channels_triggering_mask == 0 ||  // No channel, probably external trigger
          channels_triggering_mask == 1 ||  // Channel 0
          channels_triggering_mask == 2 ||  // Channel 1
          channels_triggering_mask == 4 ||  // Channel 2
          channels_triggering_mask == 8 ||  // Channel 3
          channels_triggering_mask == 3 ||  // Channel 0 and 1
          channels_triggering_mask == 12 || // Channel 2 and 3
          channels_triggering_mask == 15))  // All channels
    {
        absp_logger_error->warn("{} Wrong triggering mask (got: {}), enabling all channels;", log_name, channels_triggering_mask);
        channels_triggering_mask = 15;
    }

    // -------------------------------------------------------------------------
    //  Trigger settings
    // -------------------------------------------------------------------------

    // TODO: Enable this feature
    CHECKZERO(ADQ_DisarmTriggerBlocking(adq_cu_ptr, adq_num));
    // CHECKZERO(ADQ_SetupTriggerBlocking(adq_cu_ptr, adq_num, ADQ_TRIG_BLOCKING_DISABLE, 0, 0, 0));

    // TODO: Enable this feature
    CHECKZERO(ADQ_DisarmTimestampSync(adq_cu_ptr, adq_num));
    // CHECKZERO(ADQ_SetupTimestampSync(adq_cu_ptr, adq_num, ADQ_SYNCHRONIZATION_MODE_DISABLE, trigger_mode));

    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));

    absp_logger_console->info("{} Setting trigger; mode: {};", log_name, ADQ_descriptions::trigger_mode.at(trigger_mode));
    CHECKZERO(ADQ_SetTriggerMode(adq_cu_ptr, adq_num, trigger_mode));

    if (trigger_mode == ADQ_EXT_TRIGGER_MODE)
    {
        absp_logger_console->info("{} Setting external TTL trigger;", log_name);

        CHECKZERO(ADQ_SetExternTrigEdge(adq_cu_ptr, adq_num, trigger_slope));
        CHECKZERO(ADQ_SetExtTrigThreshold(adq_cu_ptr, adq_num, 1, default_trig_ext_threshold));
        // CHECKZERO(ADQ_SetExternalTriggerDelay(adq_cu_ptr, adq_num,  trig_external_delay));
    }
    else if (trigger_mode == ADQ_LEVEL_TRIGGER_MODE)
    {
        absp_logger_console->info("{} Setting channels trigger;", log_name);

        CHECKZERO(ADQ_SetTriggerMode(adq_cu_ptr, adq_num, trigger_mode));
        CHECKZERO(ADQ_SetLvlTrigLevel(adq_cu_ptr, adq_num, trigger_level));
        CHECKZERO(ADQ_SetLvlTrigEdge(adq_cu_ptr, adq_num, trigger_slope));
        CHECKZERO(ADQ_SetLvlTrigChannel(adq_cu_ptr, adq_num, channels_triggering_mask));
    }

    absp_logger_console->info("{} Trigger from device: {}; Channels triggering mask: {}; Level: {}; Edge: {};", log_name, ADQ_descriptions::trigger_mode.at(ADQ_GetTriggerMode(adq_cu_ptr, adq_num)), ADQ_GetLvlTrigChannel(adq_cu_ptr, adq_num), ADQ_GetLvlTrigLevel(adq_cu_ptr, adq_num), ADQ_descriptions::trigger_slope.at(ADQ_GetLvlTrigEdge(adq_cu_ptr, adq_num)));

    // -------------------------------------------------------------------------
    //  Input impedances
    // -------------------------------------------------------------------------
    absp_logger_console->info("{} Setting ports input impedances; trig port: {}; sync port: {};", log_name, ADQ_descriptions::input_impedance.at(trig_port_input_impedance), ADQ_descriptions::input_impedance.at(sync_port_input_impedance));

    CHECKZERO(ADQ_SetTriggerInputImpedance(adq_cu_ptr, adq_num, 1, trig_port_input_impedance));
    CHECKZERO(ADQ_SetTriggerInputImpedance(adq_cu_ptr, adq_num, 2, sync_port_input_impedance));

    // -------------------------------------------------------------------------
    //  Waveforms settings
    // -------------------------------------------------------------------------
    absp_logger_console->info("{} Setting pretrigger: {};", log_name, pretrigger);
    CHECKZERO(ADQ_SetPreTrigSamples(adq_cu_ptr, adq_num, pretrigger));

    // -------------------------------------------------------------------------
    //  Transfer settings
    // -------------------------------------------------------------------------

    buffers_size = records_number * samples_per_record;

    for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++)
    {
        buffers[channel].resize(buffers_size);

        target_buffers[channel] = buffers[channel].data();
    }
    for (unsigned int channel = GetChannelsNumber(); channel < ADQ_GETDATA_MAX_NOF_CHANNELS; channel++)
    {
        target_buffers[channel] = NULL;
    }

    target_timestamps.resize(records_number);
    target_headers.resize(records_number * ADQ14_FWDAQ_RECORD_HEADER_SIZE);

    // -------------------------------------------------------------------------
    //  Streaming setup
    // -------------------------------------------------------------------------

    CHECKZERO(ADQ_MultiRecordClose(adq_cu_ptr, adq_num));
    CHECKZERO(ADQ_MultiRecordSetup(adq_cu_ptr, adq_num, records_number, samples_per_record));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

void ABCD::ADQ14_FWDAQ::SetChannelsNumber(unsigned int n)
{
    const std::string log_name = GetName() + " " + GetModel() + "::SetChannelsNumber()";
    absp_logger_console->debug("{} Setting channels number to: {};", log_name, n);

    Digitizer::SetChannelsNumber(n);

    desired_input_ranges.resize(n, 1000);
    DC_offsets.resize(n, default_DC_offset);
    DBS_disableds.resize(n, false);
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::StartAcquisition()
{
    const std::string log_name = GetName() + " " + GetModel() + "::StartAcquisition()";
    absp_logger_console->trace("{} Starting acquisition; Trigger mode: {} (index: {});", log_name, ADQ_descriptions::trigger_mode.at(trigger_mode), trigger_mode);

    RearmTrigger();

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::RearmTrigger()
{
    const std::string log_name = GetName() + " " + GetModel() + "::RearmTrigger()";
    absp_logger_console->trace("{} Rearming trigger; Trigger mode: {} (index: {});", log_name, ADQ_descriptions::trigger_mode.at(trigger_mode), trigger_mode);

    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));
    CHECKZERO(ADQ_ArmTrigger(adq_cu_ptr, adq_num));

    if (trigger_mode == ADQ_SW_TRIGGER_MODE)
    {
        absp_logger_console->trace("{} Rearming trigger; Forcing software trigger;", log_name, ADQ_descriptions::trigger_mode.at(trigger_mode), trigger_mode);

        ForceSoftwareTrigger();
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

bool ABCD::ADQ14_FWDAQ::AcquisitionReady()
{
    const std::string log_name = GetName() + " " + GetModel() + "::AcquisitionReady()";

    const unsigned int retval = ADQ_GetAcquiredAll(adq_cu_ptr, adq_num);

    absp_logger_console->trace("{} Acquisition ready: {};", log_name, retval);

    return retval == 1 ? true : false;
}

//==============================================================================

bool ABCD::ADQ14_FWDAQ::DataOverflow()
{
    const std::string log_name = GetName() + " " + GetModel() + "::DataOverflow()";

    const unsigned int retval = ADQ_GetStreamOverflow(adq_cu_ptr, adq_num);

    absp_logger_console->debug("{} Overflow: {};", log_name, retval);

    return retval == 0 ? false : true;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::GetWaveformsFromCard(std::vector<struct event_waveform> &waveforms)
{
    const std::string log_name = GetName() + " " + GetModel() + "::GetWaveformsFromCard()";

    const int retval = ADQ_GetDataWHTS(adq_cu_ptr, adq_num,
                                       target_buffers,
                                       target_headers.data(),
                                       target_timestamps.data(),
                                       buffers_size, sizeof(int16_t),
                                       0, records_number,
                                       channels_acquisition_mask,
                                       0, samples_per_record,
                                       ADQ_TRANSFER_MODE_NORMAL);

    if (retval == 0)
    {
        absp_logger_error->error("{} Error in fetching data;", log_name);
        return DIGITIZER_FAILURE;
    }
    else
    {
        absp_logger_console->info("{} Collected all samples;", log_name);
    }

    for (unsigned int record_index = 0; record_index < records_number; record_index++)
    {
        uint8_t record_status = target_headers[record_index * ADQ14_FWDAQ_RECORD_HEADER_SIZE];

        if ((record_status & ADQ14_FWDAQ_RECORD_HEADER_MASK_LOST_RECORD) > 0 ||
            (record_status & ADQ14_FWDAQ_RECORD_HEADER_MASK_LOST_DATA) > 0)
        {
            absp_logger_error->warn("{} Lost data in record: {};", log_name, record_index);
        }
        else
        {
            const uint8_t ADQ_channel = target_headers[record_index * ADQ14_FWDAQ_RECORD_HEADER_SIZE + 2];
            const uint32_t ADQ_record_number = *((uint32_t *)(target_headers.data() + (record_index * ADQ14_FWDAQ_RECORD_HEADER_SIZE + 8)));

            absp_logger_console->trace("{} Record number as read from ADQ: {};", log_name, (unsigned int)ADQ_record_number);

            // If we see a jump backward in timestamp bigger than half the timestamp
            // range we assume that there was an overflow in the timestamp counter.
            // A smaller jump could mean that the records in the buffer are not
            // sorted according to the timestamp, that would make sense but we have
            // not verified it.
            const int64_t timestamp_negative_difference = timestamp_last - target_timestamps[record_index];

            if (timestamp_negative_difference > ADQ14_FWDAQ_TIMESTAMP_THRESHOLD)
            {
                absp_logger_error->warn("{} Detected timestamp overflow; Overflows: {}; Negative difference: {};", log_name, timestamp_overflows, (long long)timestamp_negative_difference);
                timestamp_offset += ADQ14_FWDAQ_TIMESTAMP_MAX;
                timestamp_overflows += 1;
            }

            // We do not add the offset here because we want to check the digitizer
            // behaviour and not the correction.
            timestamp_last = target_timestamps[record_index];

            const uint64_t timestamp_waveform = (target_timestamps[record_index] + timestamp_offset) << timestamp_bit_shift;

            for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++)
            {
                absp_logger_console->trace("{} Channel: {} as read from ADQ: {};", log_name, channel, (unsigned int)ADQ_channel);
                if (IsChannelEnabled(channel))
                {
                    struct event_waveform this_waveform = waveform_create(timestamp_waveform,
                                                                          channel,
                                                                          samples_per_record,
                                                                          0);

                    uint16_t *const samples = waveform_samples_get(&this_waveform);

                    for (unsigned int sample_index = 0; sample_index < samples_per_record; sample_index++)
                    {
                        const int16_t value = buffers[channel][record_index * samples_per_record + sample_index];

                        // We convert the signed integers to unsigned adding an
                        // offset, for compatibility with the rest of ABCD
                        samples[sample_index] = (value + (1 << 15));
                    }

                    waveforms.push_back(this_waveform);
                }
            }
        }
    }

    absp_logger_console->info("{} Converted all samples; Timestamp overflows: {};", log_name, timestamp_overflows);
    // ADQ_GetOverflow(adq_cu_ptr, adq_num);

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::StopAcquisition()
{
    const std::string log_name = GetName() + " " + GetModel() + "::StopAcquisition()";

    absp_logger_console->trace("{} Stopping acquisition;", log_name);

    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::ForceSoftwareTrigger()
{
    const std::string log_name = GetName() + " " + GetModel() + "::ForceSoftwareTrigger()";

    absp_logger_console->debug("{} Forcing a software trigger;", log_name);

    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));
    CHECKZERO(ADQ_ArmTrigger(adq_cu_ptr, adq_num));
    CHECKZERO(ADQ_SWTrig(adq_cu_ptr, adq_num));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::ResetOverflow()
{
    const std::string log_name = GetName() + " " + GetModel() + "::ResetOverflow()";

    absp_logger_console->info("{} Resetting a data overflow;", log_name);

    CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_OVERFLOW));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::ReadConfig(json_t *config)
{
    const std::string log_name = GetName() + " " + GetModel() + "::ReadConfig()";

    absp_logger_console->info("{} Reading configuration JSON;", log_name);

    const bool enable = json_is_true(json_object_get(config, "enable"));
    SetEnabled(enable);

    // Set again the settings that was just read, so in the case it was not present
    // the configuration becomes correct
    json_object_set_nocheck(config, "enable", json_boolean(enable));

    absp_logger_console->info("{} Card is {};", log_name, (enable ? "enabled" : "disabled"));

    const char *cstr_clock_source = json_string_value(json_object_get(config, "clock_source"));
    const std::string str_clock_source = (cstr_clock_source) ? std::string(cstr_clock_source) : std::string();

    // Looking for the settings in the description map
    const auto cs_result = map_utilities::find_item(ADQ_descriptions::clock_source, str_clock_source);

    if (cs_result != ADQ_descriptions::clock_source.end() && str_clock_source.length() > 0)
    {
        clock_source = cs_result->first;
    }
    else
    {
        clock_source = ADQ_CLOCK_INT_INTREF;
        absp_logger_error->error("{} Wrong clock source;", log_name);
    }

    json_object_set_nocheck(config, "clock_source", json_string(ADQ_descriptions::clock_source.at(clock_source).c_str()));

    absp_logger_console->info("{} Clock source: {} (index: {});", log_name, ADQ_descriptions::clock_source.at(clock_source), clock_source);

    timestamp_bit_shift = json_number_value(json_object_get(config, "timestamp_bit_shift"));
    absp_logger_console->info("{} Timestamp bit shift: {} bit;", log_name, timestamp_bit_shift);
    json_object_set_nocheck(config, "timestamp_bit_shift", json_integer(timestamp_bit_shift));

    // -------------------------------------------------------------------------
    //  Reading the trigger configuration
    // -------------------------------------------------------------------------
    json_t *trigger_config = json_object_get(config, "trigger");

    if (!json_is_object(trigger_config))
    {
        absp_logger_error->error("{} Missing \"trigger\" entry in configuration;", log_name);

        trigger_config = json_object();
        json_object_set_new_nocheck(config, "trigger", trigger_config);
    }

    const char *cstr_trigger_source = json_string_value(json_object_get(trigger_config, "source"));
    const std::string str_trigger_source = (cstr_trigger_source) ? std::string(cstr_trigger_source) : std::string();

    absp_logger_console->info("{} Trigger source: {};", log_name, str_trigger_source);

    // Looking for the settings in the description map
    const auto tm_result = map_utilities::find_item(ADQ_descriptions::trigger_mode, str_trigger_source);

    if (tm_result != ADQ_descriptions::trigger_mode.end() && str_trigger_source.length() > 0)
    {
        trigger_mode = tm_result->first;
    }
    else
    {
        trigger_mode = ADQ_LEVEL_TRIGGER_MODE;

        absp_logger_error->error("{} Invalid trigger source, got: {};", log_name, str_trigger_source);
    }

    json_object_set_nocheck(trigger_config, "source", json_string(ADQ_descriptions::trigger_mode.at(trigger_mode).c_str()));

    if (trigger_mode == ADQ_SW_TRIGGER_MODE)
    {
        // If the trigger is set to software we need to set the records number
        // to 1 otherwise the buffer will never fill up, because we try to read
        // the buffer and we reset it continuously.
        records_number = 1;
        json_object_set_nocheck(config, "records_number", json_integer(records_number));
    }

    const char *cstr_trigger_slope = json_string_value(json_object_get(trigger_config, "slope"));
    const std::string str_trigger_slope = (cstr_trigger_slope) ? std::string(cstr_trigger_slope) : std::string();

    absp_logger_console->info("{} Trigger slope: {};", log_name, str_trigger_slope);

    // Looking for the settings in the description map
    const auto ts_result = map_utilities::find_item(ADQ_descriptions::trigger_slope, str_trigger_slope);

    if (ts_result != ADQ_descriptions::trigger_mode.end() && str_trigger_slope.length() > 0)
    {
        trigger_slope = ts_result->first;
    }
    else
    {
        trigger_slope = ADQ_TRIG_SLOPE_FALLING;
        absp_logger_error->error("{} Invalid trigger slope, got: {};", log_name, str_trigger_slope);
    }

    json_object_set_nocheck(trigger_config, "slope", json_string(ADQ_descriptions::trigger_slope.at(trigger_slope).c_str()));

    // This level should be the absolute trigger level.
    // In the rest of ABCD the waveforms' samples are treated as uint16_t and we
    // are offsetting what we read from the ABCD::ADQ14_FWDAQ to convert from int16_t.
    // The user should be able to set a trigger level according to what it is
    // shown in the waveforms display, thus we should expect a uin16_t number
    // that we convert to a int16_t.
    const int level = json_integer_value(json_object_get(trigger_config, "level"));

    trigger_level = (level - 0x7FFF);

    json_object_set_nocheck(trigger_config, "level", json_integer(level));

    absp_logger_console->info("{} Trigger level: {};", log_name, trigger_level);

    const char *cstr_trigger_impedance = json_string_value(json_object_get(trigger_config, "impedance"));
    const std::string str_trigger_impedance = (cstr_trigger_impedance) ? std::string(cstr_trigger_impedance) : std::string();

    absp_logger_console->info("{} Trig port impedance: {};", log_name, str_trigger_impedance);

    // Looking for the settings in the description map
    const auto tii_result = map_utilities::find_item(ADQ_descriptions::input_impedance, str_trigger_impedance);

    if (tii_result != ADQ_descriptions::input_impedance.end() && str_trigger_impedance.length() > 0)
    {
        trig_port_input_impedance = tii_result->first;
    }
    else
    {
        trig_port_input_impedance = ADQ_IMPEDANCE_HIGH;
        absp_logger_error->error("{} Invalid trigger impedance, got: {};", log_name, str_trigger_impedance);
    }

    json_object_set_nocheck(trigger_config, "impedance", json_string(ADQ_descriptions::input_impedance.at(trig_port_input_impedance).c_str()));

    // -------------------------------------------------------------------------
    //  Starting the sync configuration
    // -------------------------------------------------------------------------
    json_t *sync_config = json_object_get(config, "sync");

    if (!json_is_object(sync_config))
    {
        absp_logger_error->error("{} Missing \"sync\" entry in configuration;", log_name);
        sync_config = json_object();
        json_object_set_new_nocheck(config, "sync", sync_config);
    }

    const char *cstr_sync_impedance = json_string_value(json_object_get(sync_config, "impedance"));
    const std::string str_sync_impedance = (cstr_sync_impedance) ? std::string(cstr_sync_impedance) : std::string();

    absp_logger_console->info("{} Sync port impedance: {};", log_name, str_sync_impedance);

    // Looking for the settings in the description map
    const auto tsii_result = map_utilities::find_item(ADQ_descriptions::input_impedance, str_sync_impedance);

    if (tsii_result != ADQ_descriptions::input_impedance.end() && str_sync_impedance.length() > 0)
    {
        sync_port_input_impedance = tsii_result->first;
    }
    else
    {
        sync_port_input_impedance = ADQ_IMPEDANCE_HIGH;
        absp_logger_error->error("{} Invalid sync impedance, got: {};", log_name, str_sync_impedance);
    }

    json_object_set_nocheck(sync_config, "impedance", json_string(ADQ_descriptions::input_impedance.at(sync_port_input_impedance).c_str()));

    // -------------------------------------------------------------------------
    //  Reading the single channels configuration
    // -------------------------------------------------------------------------
    // First resetting the channels statuses
    for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++)
    {
        SetChannelEnabled(channel, false);
        SetChannelTriggering(channel, false);
    }

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

                const bool triggering = json_is_true(json_object_get(value, "triggering"));
                absp_logger_console->info("{} Channel is {}triggering;", log_name, (triggering ? "" : "not "));
                json_object_set_new_nocheck(value, "triggering", json_boolean(triggering));

                float input_range = json_number_value(json_object_get(value, "input_range"));
                absp_logger_console->info("{} Input range {} mVpp;", log_name, input_range);
                if (input_range <= 0)
                {
                    input_range = default_input_range;
                    absp_logger_error->error("{} Input range must be greater than zero, got: {};", log_name, str_sync_impedance);
                }

                json_object_set_new_nocheck(value, "input_range", json_real(input_range));

                const int DC_offset = json_number_value(json_object_get(value, "DC_offset"));
                absp_logger_console->info("{} DC offset {} samples;", log_name, DC_offset);
                json_object_set_nocheck(value, "DC_offset", json_integer(DC_offset));

                // -------------------------------------------------------------
                //  Signal processing parameters
                // -------------------------------------------------------------
                const bool DBS_disable = json_is_true(json_object_get(value, "DBS_disable"));
                absp_logger_console->info("{} DBS disable: {};", log_name, (DBS_disable ? "true" : "false"));
                json_object_set_nocheck(value, "DBS_disable", DBS_disable ? json_true() : json_false());

                if (0 <= id && id < static_cast<int>(GetChannelsNumber()))
                {
                    SetChannelEnabled(id, enabled);
                    SetChannelTriggering(id, triggering);
                    desired_input_ranges[id] = input_range;
                    DC_offsets[id] = DC_offset;
                    DBS_disableds[id] = DBS_disable;
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
    //  Reading the waveforms settings
    // -------------------------------------------------------------------------

    pretrigger = json_integer_value(json_object_get(config, "pretrigger"));

    // TODO: Decide whether to use the external trigger delay or not
    // The documentation says that the card should be able to take care of it.
    // trigger_external_delay = pretrigger

    absp_logger_console->info("{} Pretrigger {} samples;", log_name, pretrigger);

    json_object_set_nocheck(config, "pretrigger", json_integer(pretrigger));

    const uint64_t scope_samples = json_integer_value(json_object_get(config, "scope_samples"));
    absp_logger_console->info("{} Scope samples: {} samples;", log_name, scope_samples);

    if (scope_samples < 1 || UINT32_MAX <= scope_samples)
    {
        absp_logger_error->error("{} Scope samples number out of range (got: {});", log_name, scope_samples);

        if (scope_samples < 1)
        {
            samples_per_record = 1;
        }
    }
    else
    {
        samples_per_record = scope_samples;
    }

    json_object_set_nocheck(config, "scope_samples", json_integer(samples_per_record));

    unsigned int max_records_number = 0;
    CHECKZERO(ADQ_GetMaxNofRecordsFromNofSamples(adq_cu_ptr, adq_num,
                                                 samples_per_record,
                                                 &max_records_number));

    absp_logger_console->info("{} Maximum records number: {} (for scope_samples: {});", log_name, max_records_number, samples_per_record);

    records_number = json_integer_value(json_object_get(config, "records_number"));
    absp_logger_console->info("{} Number of records: {};", log_name, records_number);
    if (records_number < 1 || max_records_number < records_number)
    {
        absp_logger_error->error("{} Records number out of range (got: {});", log_name, records_number);
        if (records_number < 1)
        {
            records_number = 1;
        }
        else if (max_records_number < records_number)
        {
            records_number = max_records_number;
        }
    }

    json_object_set_nocheck(config, "records_number", json_integer(records_number));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::SpecificCommand(json_t *json_command)
{
    const std::string log_name = GetName() + " " + GetModel() + "::SpecificCommand()";

    const char *cstr_command = json_string_value(json_object_get(json_command, "command"));
    const std::string command = (cstr_command) ? std::string(cstr_command) : std::string();

    absp_logger_console->info("{} Specific command: {};", log_name, command);

    if (command == std::string("GPIO_set_direction"))
    {
        const int port = json_number_value(json_object_get(json_command, "port"));
        const int direction = json_number_value(json_object_get(json_command, "direction"));
        const int mask = json_number_value(json_object_get(json_command, "mask"));

        absp_logger_console->info("{} Setting GPIO port: {}; direction: {}; mask: {}", log_name, port, direction, mask);

        const int result = GPIOSetDirection(port, direction, mask);

        return result;
    }
    else if (command == std::string("GPIO_pulse"))
    {
        const int port = json_integer_value(json_object_get(json_command, "port"));
        const int width = json_integer_value(json_object_get(json_command, "width"));
        const int mask = json_number_value(json_object_get(json_command, "mask"));

        absp_logger_console->info("{} Pulse of GPIO of width: {} us;", log_name, width);

        const int result = GPIOPulse(port, width, mask);

        return result;
    }
    else if (command == std::string("timestamp_reset"))
    {
        // ---------------------------------------------------------------------
        //  Timestamp reset
        // ---------------------------------------------------------------------

        const bool timestamp_reset_arm = json_is_true(json_object_get(json_command, "arm"));

        if (timestamp_reset_arm)
        {
            // -----------------------------------------------------------------
            //  Arming the timestamp reset
            // -----------------------------------------------------------------
            const char *cstr_timestamp_reset_mode = json_string_value(json_object_get(json_command, "mode"));
            const std::string str_timestamp_reset_mode = (cstr_timestamp_reset_mode) ? std::string(cstr_timestamp_reset_mode) : std::string();

            const char *cstr_timestamp_reset_source = json_string_value(json_object_get(json_command, "source"));
            const std::string str_timestamp_reset_source = (cstr_timestamp_reset_source) ? std::string(cstr_timestamp_reset_source) : std::string();

            const int result = TimestampResetArm(str_timestamp_reset_mode, str_timestamp_reset_source);

            return result;
        }
        else
        {
            // -----------------------------------------------------------------
            //  Disarming the timestamp reset
            // -----------------------------------------------------------------
            absp_logger_console->info("{} Disarming timestamp reset;", log_name);
            const int result = TimestampResetDisarm();

            return result;
        }
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::GPIOSetDirection(int port, int direction, int mask)
{
    const std::string log_name = GetName() + " " + GetModel() + "::GPIOSetDirection()";
    absp_logger_console->info("{} Setting GPIO direction: {}, mask: {};", log_name, direction, mask);

    // Only pins from 1 to 5 can be adressed.
    // The mask sets which pins should be ignored, a 1 on a bit informs that
    // that pin should be ignored. To play safe we set all the bits above the
    // 5th one to 1. Pin 1 corresponds to bit 0 and so on...
    const uint16_t safe_mask = mask | 0xFFE0;

    CHECKZERO(ADQ_EnableGPIOPort(adq_cu_ptr, adq_num, port, 1));
    CHECKZERO(ADQ_SetDirectionGPIOPort(adq_cu_ptr, adq_num, port, direction, mask));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::GPIOPulse(int port, int width, int mask)
{
    const std::string log_name = GetName() + " " + GetModel() + "::GPIOPulse()";
    absp_logger_console->info("{} Pulse on GPIO of width: {};", log_name, width);

    const uint16_t pin_value_on = 1;
    const uint16_t pin_value_off = 0;

    CHECKZERO(ADQ_WriteGPIOPort(adq_cu_ptr, adq_num, port, pin_value_on, mask));

    std::this_thread::sleep_for(std::chrono::microseconds(width));

    CHECKZERO(ADQ_WriteGPIOPort(adq_cu_ptr, adq_num, port, pin_value_off, mask));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::TimestampResetArm(std::string mode, std::string source)
{
    const std::string log_name = GetName() + " " + GetModel() + "::TimestampResetArm()";

    // -----------------------------------------------------------------
    //  Arming the timestamp reset
    // -----------------------------------------------------------------
    unsigned int timestamp_reset_mode = ADQ_SYNCHRONIZATION_MODE_FIRST;

    absp_logger_console->info("{} Timestamp reset mode: {};", log_name, mode);

    // Looking for the settings in the description map
    const auto tsm_result = map_utilities::find_item(ADQ_descriptions::ADQ14_timestamp_synchronization_mode, mode);

    if (tsm_result != ADQ_descriptions::ADQ14_timestamp_synchronization_mode.end() && mode.length() > 0)
    {
        timestamp_reset_mode = tsm_result->first;
    }
    else
    {
        absp_logger_error->error("{} Invalid timestamp reset mode, got: {};", mode);

        return DIGITIZER_FAILURE;
    }

    unsigned int timestamp_reset_source = ADQ_EVENT_SOURCE_SYNC;

    absp_logger_console->info("{} Timestamp synchronizaiton source: {};", log_name, source);

    // Looking for the settings in the description map
    const auto tss_result = map_utilities::find_item(ADQ_descriptions::ADQ14_timestamp_synchronization_source, source);

    if (tss_result != ADQ_descriptions::ADQ14_timestamp_synchronization_source.end() && source.length() > 0)
    {
        timestamp_reset_source = tss_result->first;
    }
    else
    {
        absp_logger_error->error("{} Invalid timestamp reset source, got: {};", log_name, source);

        return DIGITIZER_FAILURE;
    }

    absp_logger_console->info("{} Arming timestamp synchronization; mode: {}; source: {};", log_name, ADQ_descriptions::ADQ14_timestamp_synchronization_mode.at(timestamp_reset_mode), ADQ_descriptions::ADQ14_timestamp_synchronization_source.at(timestamp_reset_source));

    CHECKZERO(ADQ_DisarmTimestampSync(adq_cu_ptr, adq_num));
    CHECKZERO(ADQ_SetupTimestampSync(adq_cu_ptr, adq_num, timestamp_reset_mode, timestamp_reset_source));
    CHECKZERO(ADQ_ArmTimestampSync(adq_cu_ptr, adq_num));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::TimestampResetDisarm()
{
    const std::string log_name = GetName() + " " + GetModel() + "::TimestampResetDisarm()";
    absp_logger_console->info("{} Disarming timestamp reset;", log_name);

    CHECKZERO(ADQ_DisarmTimestampSync(adq_cu_ptr, adq_num));

    return DIGITIZER_SUCCESS;
}

//==============================================================================
