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
#include "ADQ214.hpp"

const int ABCD::ADQ214::default_trigger_output_width = 20;

ABCD::ADQ214::ADQ214(void *adq, int num) : ABCD::Digitizer(),
                                           adq_cu_ptr(adq),
                                           adq_num(num)
{
    SetModel("ADQ214");

    const std::string log_name = GetModel() + "::ADQ214()";
    absp_logger_console->info("{}", log_name);

    SetEnabled(false);

    clock_source = ADQ_CLOCK_INT_INTREF;

    trigger_mode = ADQ_LEVEL_TRIGGER_MODE;
    trigger_slope = ADQ_TRIG_SLOPE_FALLING;
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

ABCD::ADQ214::~ADQ214()
{
    const std::string log_name = GetName() + " " + GetModel() + "::~ADQ214()";
    absp_logger_console->info("{}", log_name);
}

//==============================================================================

int ABCD::ADQ214::Initialize()
{
    std::string log_name = GetModel() + "::Initialize()";
    absp_logger_console->info("{}", log_name);

    // The vendor says that these should be used only for USB digitizers
    // CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_POWER_ON));
    // CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_COMMUNICATION));

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
    absp_logger_console->info("{} ADQ214 Revision: {}", log_name, string_revision);
    absp_logger_console->info("{} Channels number: {}; ADC cores: {}; DBS instances: {}", log_name, GetChannelsNumber(), adc_cores, dbs_inst);
    absp_logger_console->info("{} Has adjustable input range: {}; Has adjustable offset: {}", log_name, (ADQ_HasAdjustableInputRange(adq_cu_ptr, adq_num) > 0 ? "true" : "false"), (ADQ_HasAdjustableBias(adq_cu_ptr, adq_num) > 0 ? "true" : "false"));

    for (auto &pair : ADQ_descriptions::ADQ214_temperatures)
    {
        const double temperature = ADQ_GetTemperature(adq_cu_ptr, adq_num, pair.first) / 256.0;
        absp_logger_console->info("{} {} temperature: {};", log_name, pair.second, temperature);
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ214::Configure()
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
    absp_logger_console->info("{} Setting PLL divider; PLL_divider: {};", log_name, PLL_divider);
    // This has to go before the SetClockSource
    CHECKZERO(ADQ_SetPllFreqDivider(adq_cu_ptr, adq_num, PLL_divider));

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
    channels_analog_front_end_mask = 0;

    for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++)
    {
        absp_logger_console->info("{} Channel: {}; Enabled: {}; Triggering: {}", log_name, channel, (IsChannelEnabled(channel) ? "true" : "false"), (IsChannelTriggering(channel) ? "true" : "false"));

        int gain;
        int offset;

        // Channel indexing starts from 1
        CHECKZERO(ADQ_GetGainAndOffset(adq_cu_ptr, adq_num, channel + 1, &gain, &offset));

        // This is just a value added to the ADC conversion and not a physical offset
        // CHECKZERO(ADQ_SetGainAndOffset(adq_cu_ptr, adq_num, channel + 1, gain, 0));

        // This is not supported in the ADQ214
        // CHECKZERO(ADQ_SetAdjustableBias(adq_cu_ptr, adq_num, channel + 1, 30000));

        uint8_t analog_front_end_mode;

        CHECKZERO(ADQ_GetAfeSwitch(adq_cu_ptr, adq_num, channel + 1, &analog_front_end_mode));

        absp_logger_console->info("{} Channel: {}; Gain: {}; Offset: {}; Analog front end mode: {};", log_name, channel, gain, offset, (unsigned int)analog_front_end_mode);

        if (IsChannelTriggering(channel))
        {
            channels_triggering_mask += (1 << channel);
        }
        if (IsChannelEnabled(channel))
        {
            channels_acquisition_mask += (1 << channel);
        }
        if (analog_front_end_couplings[channel] == ADQ_ANALOG_FRONT_END_COUPLING_DC)
        {
            channels_analog_front_end_mask += (1 << channel);
        }

        // Enabling LF amplification on the channels
        // TODO Discover what this means
        channels_analog_front_end_mask += (1 << (channel + 2));
    }

    absp_logger_console->info("{} Channels acquisition mask: {}; Channels triggering mask: {}; Channels analog front end mask: {}", log_name, (unsigned int)channels_acquisition_mask, channels_triggering_mask, channels_analog_front_end_mask);

    // Only a set of possible masks are allowed, if the mask is wrong we use
    // the most conservative option of enabling all channels.
    if (!(channels_triggering_mask == 0 || // No channel, probably external trigger
          channels_triggering_mask == 1 || // Channel 0
          channels_triggering_mask == 2 || // Channel 1
          channels_triggering_mask == 3))  // All channels
    {
        absp_logger_error->warn("{} Wrong triggering mask (got: {}), enabling all channels;", log_name, channels_triggering_mask);
        channels_triggering_mask = 3;
    }

    CHECKZERO(ADQ_SetAfeSwitch(adq_cu_ptr, adq_num, channels_analog_front_end_mask));

    // -------------------------------------------------------------------------
    //  Trigger settings
    // -------------------------------------------------------------------------

    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));

    absp_logger_console->info("{} Setting trigger; mode: {};", log_name, ADQ_descriptions::trigger_mode.at(trigger_mode));
    CHECKZERO(ADQ_SetTriggerMode(adq_cu_ptr, adq_num, trigger_mode));

    if (trigger_mode == ADQ_EXT_TRIGGER_MODE)
    {
        absp_logger_console->info("{} Setting external TTL trigger;", log_name);

        CHECKZERO(ADQ_SetExternTrigEdge(adq_cu_ptr, adq_num, trigger_slope));
        // CHECKZERO(ADQ_SetExternalTriggerDelay(adq_cu_ptr, adq_num,  trigger_external_delay));
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
    //  Trigger output
    // -------------------------------------------------------------------------
    absp_logger_console->info("{} Trigger output: {}; mode: {}; width: {};", log_name, (trigger_output_enable ? "true" : "false"), trigger_output_mode, trigger_output_width);

    if (trigger_output_enable)
    {
        // WriteTrig() needs to be set to 1, in order to enable the trigger output
        CHECKZERO(ADQ_WriteTrig(adq_cu_ptr, adq_num, 1));
        CHECKZERO(ADQ_SetConfigurationTrig(adq_cu_ptr, adq_num, trigger_output_mode,
                                           trigger_output_width, 0));

        absp_logger_console->info("{} Trigger output enabled;", log_name);
    }

    // -------------------------------------------------------------------------
    //  Waveforms settings
    // -------------------------------------------------------------------------

    // Skip every other N samples, it seems like a decimation on the acquisiton frequency
    // CHECKZERO(ADQ_SetSampleSkip(adq_cu_ptr, adq_num, 1));

    // Disable samples decimation, it seems like a downscale on the single samples
    // CHECKZERO(ADQ_SetSampleDecimation(adq_cu_ptr, adq_num, 0));

    // Set packed data format, it should make the communication faster and the
    // library should unpack automatically the data.
    CHECKZERO(ADQ_SetDataFormat(adq_cu_ptr, adq_num, ADQ214_DATA_FORMAT_PACKED_14BIT));

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
    target_headers.resize(records_number * ADQ214_RECORD_HEADER_SIZE / sizeof(uint32_t), 0);

    // -------------------------------------------------------------------------
    //  Streaming setup
    // -------------------------------------------------------------------------

    CHECKZERO(ADQ_MultiRecordClose(adq_cu_ptr, adq_num));
    CHECKZERO(ADQ_MultiRecordSetup(adq_cu_ptr, adq_num, records_number, samples_per_record));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

void ABCD::ADQ214::SetChannelsNumber(unsigned int n)
{
    const std::string log_name = GetName() + " " + GetModel() + "::SetChannelsNumber()";
    absp_logger_console->debug("{} Setting channels number to: {};", log_name, n);

    Digitizer::SetChannelsNumber(n);

    analog_front_end_couplings.resize(n, ADQ_ANALOG_FRONT_END_COUPLING_DC);
}

//==============================================================================

int ABCD::ADQ214::StartAcquisition()
{
    const std::string log_name = GetName() + " " + GetModel() + "::StartAcquisition()";
    absp_logger_console->trace("{} Starting acquisition; Trigger mode: {} (index: {});", log_name, ADQ_descriptions::trigger_mode.at(trigger_mode), trigger_mode);

    RearmTrigger();

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ214::RearmTrigger()
{
    const std::string log_name = GetName() + " " + GetModel() + "::RearmTrigger()";
    absp_logger_console->trace("{} Rearming trigger; Trigger mode: {} (index: {});", log_name, ADQ_descriptions::trigger_mode.at(trigger_mode), trigger_mode);

    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));
    CHECKZERO(ADQ_ArmTrigger(adq_cu_ptr, adq_num));

    if (trigger_mode == ADQ_SW_TRIGGER_MODE)
    {
        ForceSoftwareTrigger();
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

bool ABCD::ADQ214::AcquisitionReady()
{
    const std::string log_name = GetName() + " " + GetModel() + "::AcquisitionReady()";

    const unsigned int retval = ADQ_GetAcquiredAll(adq_cu_ptr, adq_num);

    absp_logger_console->trace("{} Acquisition ready: {};", log_name, retval);

    return retval == 1 ? true : false;
}

//==============================================================================

bool ABCD::ADQ214::DataOverflow()
{
    const std::string log_name = GetName() + " " + GetModel() + "::DataOverflow()";

    const unsigned int retval = ADQ_GetStreamOverflow(adq_cu_ptr, adq_num);

    absp_logger_console->debug("{} Overflow: {};", log_name, retval);

    return retval == 0 ? false : true;
}

//==============================================================================

int ABCD::ADQ214::GetWaveformsFromCard(std::vector<struct event_waveform> &waveforms)
{
    const std::string log_name = GetName() + " " + GetModel() + "::GetWaveformsFromCard()";

    // The headers are described in the document:
    //   11-0701-C-Trigger_ApplicationNote.pdf
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
        const size_t header_start = record_index * (ADQ214_RECORD_HEADER_SIZE / sizeof(uint32_t));

        if (absp_logger_console->should_log(spdlog::level::debug))
        {
            for (unsigned int i = 0; i < (ADQ214_RECORD_HEADER_SIZE / sizeof(uint32_t)); i++)
            {
                const uint32_t word = target_headers[header_start + i];
                absp_logger_console->debug("{} Record header word: [{}] {:08x};", log_name, i, word);
            }

            absp_logger_console->debug("{} Sub-sample time resolution bits: {:01x};", ((target_headers[header_start + 2] & ADQ214_EXTENDED_TIMESTAMP_BITMASK) >> ADQ214_EXTENDED_TIMESTAMP_OFFSET));
            absp_logger_console->debug("{} Last timestamp bit: {:01x};", (target_timestamps[record_index] & 0b1));
        }

        // If we see a jump backward in timestamp bigger than half the timestamp
        // range we assume that there was an overflow in the timestamp counter.
        // A smaller jump could mean that the records in the buffer are not
        // sorted according to the timestamp, that would make sense but we have
        // not verified it.
        const int64_t timestamp_negative_difference = timestamp_last - target_timestamps[record_index];

        if (timestamp_negative_difference > ADQ214_TIMESTAMP_THRESHOLD)
        {
            absp_logger_error->warn("{} Detected timestamp overflow; Overflows: {}; Negative difference: {};", log_name, timestamp_overflows, (long long)timestamp_negative_difference);
            timestamp_offset += ADQ214_TIMESTAMP_MAX;
            timestamp_overflows += 1;
        }

        // The board provides an extended timestamp with a sub-sampling resolution, only when using an external trigger.
        // We then shift the timestamp to fit the extended bits.
        const uint64_t timestamp_basic = target_timestamps[record_index];
        const uint64_t timestamp_extended = ((target_headers[header_start + 2] & ADQ214_EXTENDED_TIMESTAMP_BITMASK) >> ADQ214_EXTENDED_TIMESTAMP_OFFSET);

        // We do not add the offset here because we want to check the digitizer
        // behaviour and not the correction.
        timestamp_last = timestamp_basic;

        const uint64_t timestamp_waveform = ((timestamp_basic << ADQ214_EXTENDED_TIMESTAMP_NUMBER) + timestamp_extended + timestamp_offset) << timestamp_bit_shift;

        // According to 11-0701-C-Trigger_ApplicationNote.pdf the header should also contain the timestamp
        const uint64_t timestamp_from_header = target_headers[header_start + 6] + (static_cast<uint64_t>(target_headers[header_start + 7]) << 32);
        absp_logger_console->trace("{} Timestamp basic: {:016x};", log_name, timestamp_basic);
        absp_logger_console->trace("{} Timestamp from header: {:016x};", log_name, timestamp_from_header);
        absp_logger_console->trace("{} Timestamp extended: {:016x};", log_name, timestamp_extended);
        absp_logger_console->trace("{} Timestamp waveform: {:016x};", log_name, timestamp_waveform);

        for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++)
        {
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

    absp_logger_console->info("{} Converted all samples; Timestamp overflows: {};", log_name, timestamp_overflows);
    // ADQ_GetOverflow(adq_cu_ptr, adq_num);

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ214::StopAcquisition()
{
    const std::string log_name = GetName() + " " + GetModel() + "::StopAcquisition()";

    absp_logger_console->trace("{} Stopping acquisition;", log_name);

    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ214::ForceSoftwareTrigger()
{
    const std::string log_name = GetName() + " " + GetModel() + "::ForceSoftwareTrigger()";

    absp_logger_console->debug("{} Forcing a software trigger;", log_name);

    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));
    CHECKZERO(ADQ_ArmTrigger(adq_cu_ptr, adq_num));
    CHECKZERO(ADQ_SWTrig(adq_cu_ptr, adq_num));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ214::ResetOverflow()
{
    const std::string log_name = GetName() + " " + GetModel() + "::ResetOverflow()";

    absp_logger_console->info("{} Resetting a data overflow;", log_name);

    CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_OVERFLOW));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ214::ReadConfig(json_t *config)
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

    const int raw_PLL_divider = json_integer_value(json_object_get(config, "PLL_divider"));

    if (2 <= raw_PLL_divider && raw_PLL_divider <= 20)
    {
        PLL_divider = raw_PLL_divider;
    }
    else
    {
        PLL_divider = 2;
        absp_logger_error->error("{} Invalid PLL divider (got: {}), setting to default value {};", log_name, raw_PLL_divider, PLL_divider);
    }

    json_object_set_nocheck(config, "PLL_divider", json_integer(PLL_divider));

    absp_logger_console->info("{} PLL divider: {};", log_name, PLL_divider);

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
    // are offsetting what we read from the ADQ214 to convert from int16_t.
    // The user should be able to set a trigger level according to what it is
    // shown in the waveforms display, thus we should expect a uin16_t number
    // that we convert to a int16_t.
    const int level = json_integer_value(json_object_get(trigger_config, "level"));

    trigger_level = (level - 0x7FFF);

    json_object_set_nocheck(trigger_config, "level", json_integer(level));

    absp_logger_console->info("{} Trigger level: {};", log_name, trigger_level);

    // -------------------------------------------------------------------------
    //  Reading the trigger output configuration
    // -------------------------------------------------------------------------
    json_t *trigger_output_config = json_object_get(config, "trigger_output");

    if (!json_is_object(trigger_output_config))
    {
        absp_logger_error->error("{} Missing \"trigger_output\" entry in configuration;", log_name);

        trigger_output_config = json_object();
        json_object_set_new_nocheck(config, "trigger_output", trigger_output_config);
    }

    trigger_output_enable = json_is_true(json_object_get(trigger_output_config, "enable"));
    absp_logger_console->info("{} Trigger output is {};", log_name, (trigger_output_enable ? "enabled" : "disabled"));
    json_object_set_nocheck(trigger_output_config, "enable", trigger_output_enable ? json_true() : json_false());

    const char *cstr_trigger_output_source = json_string_value(json_object_get(trigger_output_config, "source"));
    const std::string str_trigger_output_source = (cstr_trigger_output_source) ? std::string(cstr_trigger_output_source) : std::string();

    // Looking for the settings in the description map
    const auto tom_result = map_utilities::find_item(ADQ_descriptions::ADQ214_trigger_output_mode, str_trigger_output_source);

    if (tom_result != ADQ_descriptions::ADQ214_trigger_output_mode.end() && str_trigger_output_source.length() > 0)
    {
        trigger_output_mode = tom_result->first;
    }
    else
    {
        trigger_output_mode = ADQ_ADQ214_TRIGGER_OUTPUT_MODE_DISABLE;
        absp_logger_error->error("{} Invalid trigger output source, got: {};", log_name, str_trigger_output_source);
    }

    if (!trigger_output_enable)
    {
        absp_logger_console->info("{} Trigger output: forcing disabled;", log_name);

        trigger_output_mode = ADQ_ADQ214_TRIGGER_OUTPUT_MODE_DISABLE;
    }

    absp_logger_console->info("{} Trigger output source: {};", log_name, str_trigger_output_source);
    json_object_set_nocheck(trigger_output_config, "source", json_string(ADQ_descriptions::ADQ214_trigger_output_mode.at(trigger_output_mode).c_str()));

    trigger_output_width = json_number_value(json_object_get(trigger_output_config, "width"));

    if (trigger_output_width < ABCD::ADQ214::default_trigger_output_width)
    {
        trigger_output_width = ABCD::ADQ214::default_trigger_output_width;
    }

    absp_logger_console->info("{} Trigger output width: {};", log_name, trigger_output_width);

    json_object_set_nocheck(trigger_output_config, "width", json_integer(trigger_output_width));

    // -------------------------------------------------------------------------
    //  Reading the single channels configuration
    // -------------------------------------------------------------------------
    // First resetting the channels statuses
    for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++)
    {
        SetChannelEnabled(channel, false);
        SetChannelTriggering(channel, false);
        analog_front_end_couplings[channel] = ADQ_ANALOG_FRONT_END_COUPLING_DC;
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

                const char *cstr_coupling = json_string_value(json_object_get(value, "coupling"));
                const std::string str_coupling = (cstr_coupling) ? std::string(cstr_coupling) : std::string();

                absp_logger_console->info("{} Channel coupling: {};", log_name, str_coupling);

                // Looking for the settings in the description map
                const auto cp_result = map_utilities::find_item(ADQ_descriptions::analog_front_end_coupling, str_coupling);

                unsigned int analog_front_end_coupling = ADQ_ANALOG_FRONT_END_COUPLING_DC;

                if (cp_result != ADQ_descriptions::analog_front_end_coupling.end() && str_coupling.length() > 0)
                {
                    analog_front_end_coupling = cp_result->first;
                }
                else
                {
                    analog_front_end_coupling = ADQ_ANALOG_FRONT_END_COUPLING_DC;
                    absp_logger_error->error("{} Invalid analog front end coupling, got: {};", log_name, str_coupling);
                }

                json_object_set_nocheck(value, "coupling", json_string(ADQ_descriptions::analog_front_end_coupling.at(analog_front_end_coupling).c_str()));

                if (0 <= id && id < static_cast<int>(GetChannelsNumber()))
                {
                    SetChannelEnabled(id, enabled);
                    SetChannelTriggering(id, triggering);
                    analog_front_end_couplings[id] = analog_front_end_coupling;
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

    const uint32_t max_samples_per_record = ADQ_GetMaxBufferSize(adq_cu_ptr, adq_num);

    const uint64_t scope_samples = json_integer_value(json_object_get(config, "scope_samples"));
    absp_logger_console->info("{} Scope samples: {} samples;", log_name, scope_samples);

    if (scope_samples < 1 || max_samples_per_record < scope_samples)
    {
        absp_logger_error->error("{} Scope samples number out of range (got: {});", log_name, scope_samples);

        if (scope_samples < 1)
        {
            samples_per_record = 1;
        }
        else if (max_samples_per_record < scope_samples)
        {
            samples_per_record = max_samples_per_record;
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

int ABCD::ADQ214::SpecificCommand(json_t *json_command)
{
    const std::string log_name = GetName() + " " + GetModel() + "::SpecificCommand()";

    const char *cstr_command = json_string_value(json_object_get(json_command, "command"));
    const std::string command = (cstr_command) ? std::string(cstr_command) : std::string();

    absp_logger_console->info("{} Specific command: {};", log_name, command);

    if (command == std::string("GPIO_set_direction"))
    {
        const unsigned int pin = json_number_value(json_object_get(json_command, "pin"));

        const char *cstr_direction = json_string_value(json_object_get(json_command, "direction"));
        const std::string direction = (cstr_direction) ? std::string(cstr_direction) : std::string();

        // Mask has an inverted logic
        const uint16_t mask = ~(1 << pin);
        uint16_t pins_directions = 0;

        if (direction == std::string("input"))
        {
            pins_directions = 0;
        }
        else if (direction == std::string("output"))
        {
            pins_directions = (1 << pin);
        }

        absp_logger_console->info("{} Setting GPIO pin: {}; direction: {}; mask: {}", log_name, pin, direction, mask);

        GPIOSetDirection(pins_directions, mask);
    }
    else if (command == std::string("GPIO_write"))
    {
        const unsigned int pin = json_number_value(json_object_get(json_command, "pin"));
        const unsigned int value = json_number_value(json_object_get(json_command, "value"));

        // Mask has an inverted logic
        const uint16_t mask = ~(1 << pin);
        const uint16_t pins_values = (value > 0) ? (1 << pin) : 0;

        absp_logger_console->info("{} GPIO write: pins: {}; mask: {};", log_name, pin, mask);

        GPIOWrite(pins_values, mask);
    }
    else if (command == std::string("GPIO_pulse"))
    {
        const unsigned int pin = json_number_value(json_object_get(json_command, "pin"));
        const int width = json_number_value(json_object_get(json_command, "width"));
        // Mask has an inverted logic
        const uint16_t mask = ~(1 << pin);

        absp_logger_console->info("{} Pulse of GPIO of width: {} us;", log_name, width);

        GPIOPulse(width, mask);
    }
    else if (command == std::string("timestamp_reset_mode"))
    {
        const char *cstr_mode = json_string_value(json_object_get(json_command, "mode"));
        const std::string mode = (cstr_mode) ? std::string(cstr_mode) : std::string();

        absp_logger_console->info("{} Timestamp reset mode: {} us;", log_name, mode);

        TimestampResetMode(mode);
    }
    else if (command == std::string("GPIO_read"))
    {
        const int read_pins_values = ADQ_ReadGPIO(adq_cu_ptr, adq_num);

        absp_logger_console->info("{} GPIO read pins values: {:x};", log_name, read_pins_values);
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ214::GPIOSetDirection(unsigned int pins_directions, unsigned int mask)
{
    const std::string log_name = GetName() + " " + GetModel() + "::GPIOSetDirection()";
    absp_logger_console->info("{} Setting GPIO direction: {}, mask: {};", log_name, pins_directions, mask);

    // Only pins from 1 to 5 can be adressed.
    // The mask sets which pins should be ignored, a 1 on a bit informs that
    // that pin should be ignored. To play safe we set all the bits above the
    // 5th one to 1. Pin 1 corresponds to bit 0 and so on...
    const uint16_t safe_mask = mask | 0xFFE0;

    // FIXME: Check the return value
    // The reference guide says: The state as a bit field, where bit 0
    // corresponds to the __value__ of GPIO pin 1, bit 1 to pin 2, and so on.
    // Maybe they intend "direction" instead of "value"
    ADQ_SetDirectionGPIO(adq_cu_ptr, adq_num, pins_directions, safe_mask);

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ214::GPIOWrite(unsigned int pins_values, unsigned int mask)
{
    const std::string log_name = GetName() + " " + GetModel() + "::GPIOWrite()";

    absp_logger_console->info("{} GPIO write: pins_values: {}; mask: {};", log_name, pins_values, mask);

    // Check notes of function GPIOSetDirection()
    const uint16_t safe_mask = mask | 0xFFE0;

    // FIXME: Check the return value
    ADQ_WriteGPIO(adq_cu_ptr, adq_num, pins_values, safe_mask);

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ214::GPIOPulse(unsigned int width, unsigned int mask)
{
    const std::string log_name = GetName() + " " + GetModel() + "::GPIOPulse()";
    absp_logger_console->info("{} Pulse on GPIO of width: {};", log_name, width);

    // Check notes of function GPIOSetDirection()
    const uint16_t safe_mask = mask | 0xFFE0;

    // All pins to ON
    const uint16_t pin_values_on = 0x1F;
    const uint16_t pin_values_off = 0;

    bool is_success = (GPIOWrite(pin_values_on, safe_mask) == DIGITIZER_SUCCESS);

    std::this_thread::sleep_for(std::chrono::microseconds(width));

    is_success = is_success && (GPIOWrite(pin_values_off, safe_mask) == DIGITIZER_SUCCESS);

    return (is_success ? DIGITIZER_SUCCESS : DIGITIZER_FAILURE);
}

//==============================================================================

int ABCD::ADQ214::TimestampResetMode(std::string mode)
{
    const std::string log_name = GetName() + " " + GetModel() + "::TimestampResetMode()";
    absp_logger_console->info("{} Timestamp reset mode: {};", log_name, mode);

    int restart_mode = 1;

    if (mode == std::string("pulse"))
    {
        restart_mode = 0;
    }
    else if (mode == std::string("now"))
    {
        restart_mode = 1;
    }

    const int return_value = ADQ_ResetTrigTimer(adq_cu_ptr, adq_num, restart_mode);

    if (!return_value)
    {
        return DIGITIZER_FAILURE;
    }
    else
    {
        return DIGITIZER_SUCCESS;
    }
}

//==============================================================================
