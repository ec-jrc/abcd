#include <cstdlib>
#include <cinttypes>

#include <fstream>
#include <stdexcept>

#include <chrono>
#include <thread>
#include <unistd.h>

extern "C" {
#include <jansson.h>

#include "utilities_functions.h"
}

#include "map_utilities.hpp"

#define LINUX
#include "ADQAPI.h"
#include "ADQ_descriptions.hpp"
#include "ADQ_utilities.hpp"
#include "ADQ412.hpp"

ABCD::ADQ412::ADQ412(void* adq, int num) : ABCD::Digitizer(),
                                           adq_cu_ptr(adq),
                                           adq_num(num)
{
    SetModel("ADQ412");

    const std::string log_name = GetModel() + "::ADQ412()";
    absp_logger_console->info("{}", log_name);

    SetEnabled(false);

    clock_source = ADQ_CLOCK_INT_INTREF;

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
}

//==============================================================================

ABCD::ADQ412::~ADQ412() {
    const std::string log_name = GetName() + " " + GetModel() + "::~ADQ412()";
    absp_logger_console->info("{}", log_name);
}

//==============================================================================

int ABCD::ADQ412::Initialize()
{
    std::string log_name = GetModel() + "::Initialize()";
    absp_logger_console->info("{}", log_name);

    // The vendor says that these should be used only for USB digitizers
    //CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_POWER_ON));
    //CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_COMMUNICATION));

    const char *board_name = ADQ_GetBoardSerialNumber(adq_cu_ptr, adq_num);

    if (!board_name) {
        absp_logger_error->error("{} Error in getting the serial number", log_name);

        SetName("");
    } else {
        SetName(board_name);
    }

    const unsigned int number_of_channels = ADQ_GetNofChannels(adq_cu_ptr, adq_num);

    SetChannelsNumber(number_of_channels);

    unsigned int dbs_inst = 0;
    CHECKZERO(ADQ_GetNofDBSInstances(adq_cu_ptr, adq_num, &dbs_inst));

    absp_logger_console->info("{} Initialized board", log_name);
    absp_logger_console->info("{} Card name (serial number): {}; Product name: {}; Card option: {}; Motherboard option: {};", log_name, GetName(), ADQ_GetBoardProductName(adq_cu_ptr, adq_num), ADQ_GetCardOption(adq_cu_ptr, adq_num), ADQ_GetADQDSPOption(adq_cu_ptr, adq_num));
    absp_logger_console->info("{} USB address: {}; PCIe address: {};", log_name, ADQ_GetUSBAddress(adq_cu_ptr, adq_num), ADQ_GetPCIeAddress(adq_cu_ptr, adq_num));
    absp_logger_console->info("{} ADQAPI revision: {}; ", log_name, ADQAPI_GetRevisionString());
    uint32_t *revision = ADQ_GetRevision(adq_cu_ptr, adq_num);
    std::string string_revision = "";

    for (int i = 0; i < 6; i++) {
        string_revision += std::to_string((unsigned int)revision[i]) + ", ";
    }
    absp_logger_console->info("{} ADQ412 Revision: {}", log_name, string_revision);
    absp_logger_console->info("{} Channels number: {}; DBS instances: {}", log_name, GetChannelsNumber(), dbs_inst);
    absp_logger_console->info("{} Has adjustable input range: {}; Has adjustable offset: {}", log_name, (ADQ_HasAdjustableInputRange(adq_cu_ptr, adq_num) > 0 ? "true" : "false"), (ADQ_HasAdjustableBias(adq_cu_ptr, adq_num) > 0 ? "true" : "false"));

    for (auto &pair : ADQ_descriptions::ADQ412_temperatures) {
        const double temperature = ADQ_GetTemperature(adq_cu_ptr, adq_num, pair.first) / 256.0;
        absp_logger_console->info("{} {} temperature: {};", log_name, pair.second, temperature);
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ412::Configure()
{
    const std::string log_name = GetName() + " " + GetModel() + "::Configure()";
    absp_logger_console->info("{} Configuring board;", log_name);

    if (!IsEnabled()) {
        return DIGITIZER_SUCCESS; // if card is OFF return immediately
    }

    // -------------------------------------------------------------------------
    //  Clock settings
    // -------------------------------------------------------------------------

    absp_logger_console->info("{} Setting clock; clock_source: {};", log_name, ADQ_descriptions::clock_source.at(clock_source));
    CHECKZERO(ADQ_SetClockSource(adq_cu_ptr, adq_num, clock_source));

    absp_logger_console->info("{} Clock source from device: {};", log_name, ADQ_GetClockSource(adq_cu_ptr, adq_num));
    try {
        absp_logger_console->info("{} Clock source from device: {};", log_name, ADQ_descriptions::clock_source.at(ADQ_GetClockSource(adq_cu_ptr, adq_num)));
    } catch (...) {
    }

    //pll_divider = (int)aSampling[iSampling];
    //absp_logger_console->info("{} Setting PLL divider; PLL_divider: {};", log_name, PLL_divider);
    // FIXME: I have no idea what this does
    //CHECKZERO(ADQ_SetPllFreqDivider(adq_cu_ptr,adq_num, pll_divider));
    // FIXME: This is not working
    //CHECKZERO(ADQ_SetPll(adq_cu_ptr, adq_num, pll_divider, 2, 1, 1));

    const int interleaving_mode = 0;
    absp_logger_console->info("{} Setting interleaving mode: {};", log_name, interleaving_mode);
    CHECKZERO(ADQ_SetInterleavingMode(adq_cu_ptr, adq_num, interleaving_mode)); //1 A/C 2 B/D

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

    for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++) {
        absp_logger_console->info("{} Channel: {}; Enabled: {}; Triggering: {}", log_name, channel, (IsChannelEnabled(channel) ? "true" : "false"), (IsChannelTriggering(channel) ? "true" : "false"));

        int gain;
        int offset;

        // Channel indexing starts from 1
        CHECKZERO(ADQ_GetGainAndOffset(adq_cu_ptr, adq_num, channel + 1, &gain, &offset));

        // This is just a value added to the ADC conversion and not a physical offset
        //CHECKZERO(ADQ_SetGainAndOffset(adq_cu_ptr, adq_num, channel + 1, gain, 0));

        // This might be supported in the ADQ412
        //CHECKZERO(ADQ_SetAdjustableBias(adq_cu_ptr, adq_num, channel + 1, 30000));

        absp_logger_console->info("{} Channel: {}; Gain: {}; Offset: {};", log_name, channel, gain, offset);

        if (IsChannelTriggering(channel)) {
            channels_triggering_mask += (1 << channel);
        }
        if (IsChannelEnabled(channel)) {
            channels_acquisition_mask += (1 << channel);
        }
    }

    absp_logger_console->info("{} Channels acquisition mask: {}; Channels triggering mask: {};", log_name, (unsigned int)channels_acquisition_mask, channels_triggering_mask);

    // Only a set of possible masks are allowed, if the mask is wrong we use
    // a conservative option. Only channel couples can be used to trigger,
    // because the channels are interleaved. If either one of the channel couple
    // is enabled, then the whole couple is enabled.
    if ((channels_triggering_mask & 3) == 1 || (channels_triggering_mask & 3) == 2) {
        absp_logger_error->warn("{} Wrong triggering mask for the first couple (got: {}), enabling all channels of the couple;", log_name, channels_triggering_mask & 3);

        channels_triggering_mask = channels_triggering_mask | 3;
    }
    if ((channels_triggering_mask & 12) == 4 || (channels_triggering_mask & 12) == 8) {
        absp_logger_error->warn("{} Wrong triggering mask (got: {}), enabling all channels;", log_name, channels_triggering_mask);
        channels_triggering_mask = channels_triggering_mask | 12;
    }

    // -------------------------------------------------------------------------
    //  Trigger settings
    // -------------------------------------------------------------------------

    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));

    absp_logger_console->info("{} Setting the timestamp to a continuous increment;", log_name);

    CHECKZERO(ADQ_SetTrigTimeMode(adq_cu_ptr, adq_num, 0));

    absp_logger_console->info("{} Setting trigger; mode: {};", log_name, ADQ_descriptions::trigger_mode.at(trigger_mode));
    CHECKZERO(ADQ_SetTriggerMode(adq_cu_ptr, adq_num, trigger_mode));

    if (trigger_mode == ADQ_EXT_TRIGGER_MODE) {
        absp_logger_console->info("{} Setting external TTL trigger;", log_name);

        CHECKZERO(ADQ_SetExternTrigEdge(adq_cu_ptr, adq_num, trigger_slope));
        //CHECKZERO(ADQ_SetExternalTriggerDelay(adq_cu_ptr, adq_num,  trigger_external_delay));
    } else if (trigger_mode == ADQ_LEVEL_TRIGGER_MODE) {
        absp_logger_console->info("{} Setting channels trigger;", log_name);

        CHECKZERO(ADQ_SetTriggerMode(adq_cu_ptr,  adq_num, trigger_mode));
        CHECKZERO(ADQ_SetLvlTrigLevel(adq_cu_ptr, adq_num, trigger_level));
        CHECKZERO(ADQ_SetLvlTrigEdge(adq_cu_ptr,  adq_num, trigger_slope));
        CHECKZERO(ADQ_SetLvlTrigChannel(adq_cu_ptr, adq_num, channels_triggering_mask));
    }

    absp_logger_console->info("{} Trigger from device: {}; Channels triggering mask: {}; Level: {}; Edge: {};", log_name, ADQ_descriptions::trigger_mode.at(ADQ_GetTriggerMode(adq_cu_ptr, adq_num)), ADQ_GetLvlTrigChannel(adq_cu_ptr, adq_num), ADQ_GetLvlTrigLevel(adq_cu_ptr, adq_num), ADQ_descriptions::trigger_slope.at(ADQ_GetLvlTrigEdge(adq_cu_ptr, adq_num)));

    // -------------------------------------------------------------------------
    //  Waveforms settings
    // -------------------------------------------------------------------------

    // Skip every other N samples, it seems like a decimation on the acquisiton frequency
    //CHECKZERO(ADQ_SetSampleSkip(adq_cu_ptr, adq_num, 1));

    // Disable samples decimation, it seems like a downscale on the single samples
    //CHECKZERO(ADQ_SetSampleDecimation(adq_cu_ptr, adq_num, 0));

    // Set packed data format, it should make the communication faster and the
    // library should unpack automatically the data.
    CHECKZERO(ADQ_SetDataFormat(adq_cu_ptr, adq_num, ADQ412_DATA_FORMAT_PACKED_12BIT));

    absp_logger_console->info("{} Setting pretrigger: {};", log_name, pretrigger);
    CHECKZERO(ADQ_SetPreTrigSamples(adq_cu_ptr, adq_num, pretrigger));

    // -------------------------------------------------------------------------
    //  Transfer settings
    // -------------------------------------------------------------------------

    buffers_size = records_number * samples_per_record;

    for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++) {
        buffers[channel].resize(buffers_size);

        target_buffers[channel] = buffers[channel].data();
    }
    for (unsigned int channel = GetChannelsNumber(); channel < ADQ_GETDATA_MAX_NOF_CHANNELS; channel++) {
        target_buffers[channel] = NULL;
    }

    target_timestamps.resize(records_number);
    target_headers.resize(records_number * ADQ412_RECORD_HEADER_SIZE);

    // -------------------------------------------------------------------------
    //  Streaming setup
    // -------------------------------------------------------------------------

    CHECKZERO(ADQ_MultiRecordClose(adq_cu_ptr, adq_num));
    CHECKZERO(ADQ_MultiRecordSetup(adq_cu_ptr, adq_num, records_number, samples_per_record));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ412::StartAcquisition()
{
    const std::string log_name = GetName() + " " + GetModel() + "::StartAcquisition()";
    absp_logger_console->trace("{} Starting acquisition; Trigger mode: {} (index: {});", log_name, ADQ_descriptions::trigger_mode.at(trigger_mode), trigger_mode);

    RearmTrigger();

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ412::RearmTrigger()
{
    const std::string log_name = GetName() + " " + GetModel() + "::RearmTrigger()";
    absp_logger_console->trace("{} Rearming trigger; Trigger mode: {} (index: {});", log_name, ADQ_descriptions::trigger_mode.at(trigger_mode), trigger_mode);

    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));
    CHECKZERO(ADQ_ArmTrigger(adq_cu_ptr, adq_num));

    if (trigger_mode == ADQ_SW_TRIGGER_MODE) {
        ForceSoftwareTrigger();
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

bool ABCD::ADQ412::AcquisitionReady()
{
    const std::string log_name = GetName() + " " + GetModel() + "::AcquisitionReady()";

    const unsigned int retval = ADQ_GetAcquiredAll(adq_cu_ptr, adq_num);

    absp_logger_console->trace("{} Acquisition ready: {};", log_name, retval);

    return retval == 1 ? true : false;
}

//==============================================================================

bool ABCD::ADQ412::DataOverflow()
{
    const std::string log_name = GetName() + " " + GetModel() + "::DataOverflow()";

    const unsigned int retval = ADQ_GetStreamOverflow(adq_cu_ptr, adq_num);

    absp_logger_console->debug("{} Overflow: {};", log_name, retval);

    return retval == 0 ? false : true;
}

//==============================================================================

int ABCD::ADQ412::GetWaveformsFromCard(std::vector<struct event_waveform> &waveforms)
{
    const std::string log_name = GetName() + " " + GetModel() + "::GetWaveformsFromCard()";

    // We will skip the target headers reading because we have no documentation
    // about their structure, thus we will pass just NULL to the function.
    const int retval = ADQ_GetDataWHTS(adq_cu_ptr, adq_num,
                                       target_buffers,
                                       //target_headers.data(),
                                       NULL,
                                       target_timestamps.data(),
                                       buffers_size, sizeof(int16_t),
                                       0, records_number,
                                       channels_acquisition_mask,
                                       0, samples_per_record,
                                       ADQ_TRANSFER_MODE_NORMAL);

    if (retval == 0) {
        absp_logger_error->error("{} Error in fetching data;", log_name);
        return DIGITIZER_FAILURE;
    } else {
        absp_logger_console->info("{} Collected all samples;", log_name);
    }

    for (unsigned int record_index = 0; record_index < records_number; record_index++) {
        // If we see a jump backward in timestamp bigger than half the timestamp
        // range we assume that there was an overflow in the timestamp counter.
        // A smaller jump could mean that the records in the buffer are not
        // sorted according to the timestamp, that would make sense but we have
        // not verified it.
        const int64_t timestamp_negative_difference = timestamp_last - target_timestamps[record_index];

        if (timestamp_negative_difference > ADQ412_TIMESTAMP_THRESHOLD) {
            absp_logger_error->warn("{} Detected timestamp overflow; Overflows: {}; Negative difference: {};", log_name, timestamp_overflows, (long long)timestamp_negative_difference);
            timestamp_offset += ADQ412_TIMESTAMP_MAX;
            timestamp_overflows += 1;
        }

        // We do not add the offset here because we want to check the digitizer
        // behaviour and not the correction.
        timestamp_last = target_timestamps[record_index];

        const uint64_t timestamp_waveform = target_timestamps[record_index] + timestamp_offset;

        for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++) {
            if (IsChannelEnabled(channel)) {
                struct event_waveform this_waveform = waveform_create(timestamp_waveform,
                                                                      channel,
                                                                      samples_per_record,
                                                                      0);

                uint16_t * const samples = waveform_samples_get(&this_waveform);

                for (unsigned int sample_index = 0; sample_index < samples_per_record; sample_index++) {
                    const int16_t value = buffers[channel][record_index * samples_per_record + sample_index];

                    // We convert the signed integers to unsigned adding an
                    // offset, for compatibility with the rest of ABCD
                    samples[sample_index] = (value + (1 << 15));
                }

                waveforms.push_back(this_waveform);
            }
        }
    }

    absp_logger_console->info("{} Converted all samples; Timestamp overflows: {};", log_name, timestamp_overflows);
    //ADQ_GetOverflow(adq_cu_ptr, adq_num);

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ412::StopAcquisition()
{
    const std::string log_name = GetName() + " " + GetModel() + "::StopAcquisition()";

    absp_logger_console->trace("{} Stopping acquisition;", log_name);

    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ412::ForceSoftwareTrigger()
{
    const std::string log_name = GetName() + " " + GetModel() + "::ForceSoftwareTrigger()";

    absp_logger_console->debug("{} Forcing a software trigger;", log_name);

    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));
    CHECKZERO(ADQ_ArmTrigger(adq_cu_ptr, adq_num));
    CHECKZERO(ADQ_SWTrig(adq_cu_ptr, adq_num));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ412::ResetOverflow()
{
    const std::string log_name = GetName() + " " + GetModel() + "::ResetOverflow()";

    absp_logger_console->info("{} Resetting a data overflow;", log_name);

    CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_OVERFLOW));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ412::ReadConfig(json_t *config)
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

    if (cs_result != ADQ_descriptions::clock_source.end() && str_clock_source.length() > 0) {
        clock_source = cs_result->first;
    } else {
        clock_source = ADQ_CLOCK_INT_INTREF;
        absp_logger_error->error("{} Wrong clock source;", log_name);
    }

    json_object_set_nocheck(config, "clock_source", json_string(ADQ_descriptions::clock_source.at(clock_source).c_str()));

    absp_logger_console->info("{} Clock source: {} (index: {});", log_name, ADQ_descriptions::clock_source.at(clock_source), clock_source);

    //const int sampling = json_integer_value(json_object_get(config, "sampling_frequency"));
    //iSampling = IndexOfClosest(sampling, sSampling);

    //if (GetVerbosity() > 0)
    //{
    //    char time_buffer[BUFFER_SIZE];
    //    time_string(time_buffer, BUFFER_SIZE, NULL);
    //    std::cout << '[' << time_buffer << "] ABCD::ADQ412::ReadConfig() ";
    //    std::cout << "Sampling: got: " << sampling << "; ";
    //    std::cout << "selected: " << sSampling[iSampling] << " (index: " << iSampling << "); ";
    //    std::cout << std::endl;
    //}

    //json_object_set_nocheck(config, "sampling_frequency", json_integer(sSampling[iSampling]));

    // -------------------------------------------------------------------------
    //  Reading the trigger configuration
    // -------------------------------------------------------------------------
    json_t *trigger_config = json_object_get(config, "trigger");

    if (!json_is_object(trigger_config)) {
        absp_logger_error->error("{} Missing \"trigger\" entry in configuration;", log_name);

        trigger_config = json_object();
        json_object_set_new_nocheck(config, "trigger", trigger_config);
    }

    const char *cstr_trigger_source = json_string_value(json_object_get(trigger_config, "source"));
    const std::string str_trigger_source = (cstr_trigger_source) ? std::string(cstr_trigger_source) : std::string();

    absp_logger_console->info("{} Trigger source: {};", log_name, str_trigger_source);

    // Looking for the settings in the description map
    const auto tm_result = map_utilities::find_item(ADQ_descriptions::trigger_mode, str_trigger_source);

    if (tm_result != ADQ_descriptions::trigger_mode.end() && str_trigger_source.length() > 0) {
        trigger_mode = tm_result->first;
    } else {
        trigger_mode = ADQ_LEVEL_TRIGGER_MODE;

        absp_logger_error->error("{} Invalid trigger source, got: {};", log_name, str_trigger_source);
    }

    json_object_set_nocheck(trigger_config, "source", json_string(ADQ_descriptions::trigger_mode.at(trigger_mode).c_str()));

    if (trigger_mode == ADQ_SW_TRIGGER_MODE) {
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

    if (ts_result != ADQ_descriptions::trigger_mode.end() && str_trigger_slope.length() > 0) {
        trigger_slope = ts_result->first;
    } else {
        trigger_slope = ADQ_TRIG_SLOPE_FALLING;
        absp_logger_error->error("{} Invalid trigger slope, got: {};", log_name, str_trigger_slope);
    }

    json_object_set_nocheck(trigger_config, "slope", json_string(ADQ_descriptions::trigger_slope.at(trigger_slope).c_str()));

    // This level should be the absolute trigger level.
    // In the rest of ABCD the waveforms' samples are treated as uint16_t and we
    // are offsetting what we read from the ABCD::ADQ412 to convert from int16_t.
    // The user should be able to set a trigger level according to what it is
    // shown in the waveforms display, thus we should expect a uin16_t number
    // that we convert to a int16_t.
    const int level = json_integer_value(json_object_get(trigger_config, "level"));

    trigger_level = (level - 0x7FFF);

    json_object_set_nocheck(trigger_config, "level", json_integer(level));

    absp_logger_console->info("{} Trigger level: {};", log_name, trigger_level);

    // -------------------------------------------------------------------------
    //  Reading the single channels configuration
    // -------------------------------------------------------------------------
    // First resetting the channels statuses
    for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++) {
        SetChannelEnabled(channel, false);
        SetChannelTriggering(channel, false);
    }

    json_t *json_channels = json_object_get(config, "channels");

    if (json_channels != NULL && json_is_array(json_channels))
    {
        size_t index;
        json_t *value;

        json_array_foreach(json_channels, index, value) {
            json_t *json_id = json_object_get(value, "id");

            if (json_id != NULL && json_is_integer(json_id)) {
                const int id = json_integer_value(json_id);

                absp_logger_console->info("{} Found channel: {};", log_name, id);

                const bool enabled = json_is_true(json_object_get(value, "enable"));

                absp_logger_console->info("{} Channel is {};", log_name, (enabled ? "disabled" : "enabled"));

                json_object_set_new_nocheck(value, "enable", json_boolean(enabled));

                const bool triggering = json_is_true(json_object_get(value, "triggering"));
                absp_logger_console->info("{} Channel is {}triggering;", log_name, (triggering ? "" : "not "));
                json_object_set_new_nocheck(value, "triggering", json_boolean(triggering));


                if (0 <= id && id < static_cast<int>(GetChannelsNumber())) {
                    SetChannelEnabled(id, enabled);
                    SetChannelTriggering(id, triggering);
                } else {
                    absp_logger_error->error("{} Channel out of range, ignoring it (got: {});", log_name, id);
                }
            } else {
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

    const uint32_t max_samples_per_record = ADQ_GetMaxBufferSize(adq_cu_ptr, adq_num);

    const uint64_t scope_samples = json_integer_value(json_object_get(config, "scope_samples"));
    absp_logger_console->info("{} Scope samples: {} samples;", log_name, scope_samples);

    if (scope_samples < 1 || max_samples_per_record < scope_samples) {
        absp_logger_error->error("{} Scope samples number out of range (got: {});", log_name, scope_samples);

        if (scope_samples < 1) {
            samples_per_record = 1;
        } else if (max_samples_per_record < scope_samples) {
            samples_per_record = max_samples_per_record;
        }
    } else {
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
    if (records_number < 1 || max_records_number < records_number) {
        absp_logger_error->error("{} Records number out of range (got: {});", log_name, records_number);
        if (records_number < 1) {
            records_number = 1;
        } else if (max_records_number < records_number) {
            records_number = max_records_number;
        }
    }

    json_object_set_nocheck(config, "records_number", json_integer(records_number));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ412::SpecificCommand(json_t *json_command)
{
    const std::string log_name = GetName() + " " + GetModel() + "::SpecificCommand()";

    const char *cstr_command = json_string_value(json_object_get(json_command, "command"));
    const std::string command = (cstr_command) ? std::string(cstr_command) : std::string();

    absp_logger_console->info("{} Specific command: {};", log_name, command);

    if (command == std::string("GPIO_set_direction")) {
        const char *cstr_direction = json_string_value(json_object_get(json_command, "direction"));
        const std::string direction = (cstr_direction) ? std::string(cstr_direction) : std::string();

        // Informing that all the pins should keep their direction, only pin 5
        // can be changed in these boards.
        const uint16_t mask = 0xF;
        uint16_t pins_directions = 0;

        if (direction == std::string("input")) {
            pins_directions = 0;
        } else if (direction == std::string("output")) {
            pins_directions = (1 << 4);
        }

        absp_logger_console->info("{} Setting GPIO direction: {}; mask: {}", log_name, direction, mask);

        CHECKZERO(ADQ_SetDirectionGPIO(adq_cu_ptr, adq_num, pins_directions, mask));
    } else if (command == std::string("GPIO_write")) {
        const int value = json_integer_value(json_object_get(json_command, "value"));

        // Informing that all the pins should ignore the command, only pin 5 can
        // be used in these boards.
        const uint16_t mask = 0xF;
        const uint16_t pin_value = (value > 0) ? (1 << 4) : 0;

        absp_logger_console->info("{} GPIO write: value: {};", log_name, pin_value);

        CHECKZERO(ADQ_WriteGPIO(adq_cu_ptr, adq_num, pin_value, mask));
    } else if (command == std::string("GPIO_pulse")) {
        const int width = json_integer_value(json_object_get(json_command, "width"));

        absp_logger_console->info("{} Pulse of GPIO of width: {} us;", log_name, width);

        // Informing that all the pins should ignore the command, only pin 5 can
        // be used in these boards.
        const uint16_t mask = 0xF;
        const uint16_t pin_value_on = (1 << 4);
        const uint16_t pin_value_off = 0;

        CHECKZERO(ADQ_WriteGPIO(adq_cu_ptr, adq_num, pin_value_on, mask));

        std::this_thread::sleep_for(std::chrono::microseconds(width));

        CHECKZERO(ADQ_WriteGPIO(adq_cu_ptr, adq_num, pin_value_off, mask));
    } else if (command == std::string("timestamp_reset")) {
        const char *cstr_mode = json_string_value(json_object_get(json_command, "mode"));
        const std::string mode = (cstr_mode) ? std::string(cstr_mode) : std::string();

        absp_logger_console->info("{} Timestamp reset mode: {} us;", log_name, mode);

        int restart_mode = 1;

        if (mode == std::string("pulse")) {
            restart_mode = 0;
        } else if (mode == std::string("now")) {
            restart_mode = 1;
        }

        CHECKZERO(ADQ_ResetTrigTimer(adq_cu_ptr, adq_num, restart_mode));
    } else if (command == std::string("GPIO_read")) {
        const int read_pins_values = ADQ_ReadGPIO(adq_cu_ptr, adq_num);

        absp_logger_console->info("{} GPIO read pins values: {:x};", log_name, read_pins_values);
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================
