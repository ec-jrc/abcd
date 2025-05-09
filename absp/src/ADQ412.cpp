#include <cstdlib>
#include <cinttypes>

#include <fstream>
#include <iostream>
#include <stdexcept>

#include <chrono>
#include <thread>
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

#define BUFFER_SIZE 32


ABCD::ADQ412::ADQ412(void* adq, int num, int Verbosity) : ABCD::Digitizer(Verbosity),
                                                          adq_cu_ptr(adq),
                                                          adq_num(num)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::ADQ412() ";
        std::cout << std::endl;
    }

    SetModel("ADQ412");

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

ABCD::ADQ412::~ADQ412() { }

//==============================================================================

int ABCD::ADQ412::Initialize()
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::Initialize() ";
        std::cout << std::endl;
    }

    // The vendor says that these should be used only for USB digitizers
    //CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_POWER_ON));
    //CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_COMMUNICATION));

    const char *board_name = ADQ_GetBoardSerialNumber(adq_cu_ptr, adq_num);

    if (!board_name) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::Initialize() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Error in getting the serial number; ";
        std::cout << std::endl;

        SetName("");
    } else {
        SetName(board_name);
    }

    const unsigned int number_of_channels = ADQ_GetNofChannels(adq_cu_ptr, adq_num);

    SetChannelsNumber(number_of_channels);

    unsigned int dbs_inst = 0;
    CHECKZERO(ADQ_GetNofDBSInstances(adq_cu_ptr, adq_num, &dbs_inst));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::Initialize() ";
        std::cout << "Initialized board; ";
        std::cout << std::endl;
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::Initialize() ";
        std::cout << "Card name (serial number): " << GetName() << "; ";
        std::cout << "Product name: " << ADQ_GetBoardProductName(adq_cu_ptr, adq_num) << "; ";
        std::cout << "Card option: " << ADQ_GetCardOption(adq_cu_ptr, adq_num) << "; ";
        std::cout << "Motheboard option: " << ADQ_GetADQDSPOption(adq_cu_ptr, adq_num) << "; ";
        std::cout << std::endl;

        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Initialize() ";
        std::cout << "USB address: " << ADQ_GetUSBAddress(adq_cu_ptr, adq_num) << "; ";
        std::cout << "PCIe address: " << ADQ_GetPCIeAddress(adq_cu_ptr, adq_num) << "; ";
        std::cout << std::endl;

        std::cout << '[' << time_buffer << "] ABCD::ADQ412::Initialize() ";
        std::cout << "ADQAPI Revision: " << ADQAPI_GetRevisionString() << "; ";
        std::cout << "ADQ412 Revision: {";
        uint32_t* revision = ADQ_GetRevision(adq_cu_ptr, adq_num);
        for (int i = 0; i < 6; i++) {
            std::cout << (unsigned int)revision[i] << ", ";
        }
        std::cout << "}; ";
        std::cout << std::endl;

        std::cout << '[' << time_buffer << "] ABCD::ADQ412::Initialize() ";
        std::cout << "Channels number: " << GetChannelsNumber() << "; ";
        std::cout << "DBS instances: " << dbs_inst << "; ";
        std::cout << std::endl;

        std::cout << '[' << time_buffer << "] ABCD::ADQ412::Initialize() ";
        std::cout << "Has adjustable input range: " << (ADQ_HasAdjustableInputRange(adq_cu_ptr, adq_num) > 0 ? "true" : "false") << "; ";
        std::cout << "Has adjustable offset: " << (ADQ_HasAdjustableBias(adq_cu_ptr, adq_num) > 0 ? "true" : "false") << "; ";
        std::cout << std::endl;

        for (auto &pair : ADQ_descriptions::ADQ412_temperatures) {
            const double temperature = ADQ_GetTemperature(adq_cu_ptr, adq_num, pair.first) / 256.0;

            std::cout << '[' << time_buffer << "] ABCD::ADQ412::Initialize() ";
            std::cout << pair.second << " temperature: " << temperature << "; ";
            std::cout << std::endl;
        }
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ412::Configure()
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::Configure() ";
        std::cout << "Configuring board; ";
        std::cout << std::endl;
    }


    if (!IsEnabled()) {
        return DIGITIZER_SUCCESS; // if card is OFF return immediately
    }

    // -------------------------------------------------------------------------
    //  Clock settings
    // -------------------------------------------------------------------------

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::Configure() ";
        std::cout << "Setting clock; ";
        std::cout << "clock_source: " << ADQ_descriptions::clock_source.at(clock_source) << "; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_SetClockSource(adq_cu_ptr, adq_num, clock_source));

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::Configure() ";
        std::cout << "Clock source from device: " << ADQ_descriptions::clock_source.at(ADQ_GetClockSource(adq_cu_ptr, adq_num)) << "; ";
        std::cout << std::endl;
    }

    //pll_divider = (int)aSampling[iSampling];
    //if (GetVerbosity() > 0)
    //{
    //    char time_buffer[BUFFER_SIZE];
    //    time_string(time_buffer, BUFFER_SIZE, NULL);
    //    std::cout << '[' << time_buffer << "] ABCD::ADQ412::Configure() ";
    //    std::cout << "Setting PLL divider; ";
    //    std::cout << "pll_divider: " << pll_divider << "; ";
    //    std::cout << std::endl;
    //}
    // FIXME: I have no idea what this does
    //CHECKZERO(ADQ_SetPllFreqDivider(adq_cu_ptr,adq_num, pll_divider));
    // FIXME: This is not working
    //CHECKZERO(ADQ_SetPll(adq_cu_ptr, adq_num, pll_divider, 2, 1, 1));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::Configure() ";
        std::cout << "Setting interleaving mode; ";
        std::cout << "interleaving_mode: " << 0 << "; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_SetInterleavingMode(adq_cu_ptr, adq_num, 0)); //1 A/C 2 B/D

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::Configure() ";
        std::cout << "Disabling test data; ";
        std::cout << std::endl;
    }

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
        int gain;
        int offset;

        // Channel indexing starts from 1
        CHECKZERO(ADQ_GetGainAndOffset(adq_cu_ptr, adq_num, channel + 1, &gain, &offset));

        // This is just a value added to the ADC conversion and not a physical offset
        //CHECKZERO(ADQ_SetGainAndOffset(adq_cu_ptr, adq_num, channel + 1, gain, 0));

        // This might be supported in the ADQ412
        //CHECKZERO(ADQ_SetAdjustableBias(adq_cu_ptr, adq_num, channel + 1, 30000));

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ412::Configure() ";
            std::cout << "Channel: " << channel << "; ";
            std::cout << "Enabled: " << (IsChannelEnabled(channel) ? "true" : "false") << "; ";
            std::cout << "Triggering: " << (IsChannelTriggering(channel) ? "true" : "false") << "; ";
            std::cout << "Gain: " << gain << "; ";
            std::cout << "Offset: " << offset << "; ";
            std::cout << std::endl;
        }

        if (IsChannelTriggering(channel)) {
            channels_triggering_mask += (1 << channel);
        }
        if (IsChannelEnabled(channel)) {
            channels_acquisition_mask += (1 << channel);
        }
    }

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::Configure() ";
        std::cout << "Channels acquisition mask: " << (unsigned int)channels_acquisition_mask << "; ";
        std::cout << "Channels triggering mask: " << channels_triggering_mask << "; ";
        std::cout << std::endl;
    }

    // Only a set of possible masks are allowed, if the mask is wrong we use
    // a conservative option. Only channel couples can be used to trigger,
    // because the channels are interleaved. If either one of the channel couple
    // is enabled, then the whole couple is enabled.
    if ((channels_triggering_mask & 3) == 1 || (channels_triggering_mask & 3) == 2) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::Configure() ";
        std::cout << WRITE_YELLOW << "WARNING" << WRITE_NC << ": Wrong triggering mask for the first couple (got: " << (channels_triggering_mask & 3) << "), enabling all channels of the couple; ";
        std::cout << std::endl;

        channels_triggering_mask = channels_triggering_mask | 3;
    }
    if ((channels_triggering_mask & 12) == 4 || (channels_triggering_mask & 12) == 8) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::Configure() ";
        std::cout << WRITE_YELLOW << "WARNING" << WRITE_NC << ": Wrong triggering mask for the second couple (got: " << (channels_triggering_mask & 12) << "), enabling all channels of the couple; ";
        std::cout << std::endl;

        channels_triggering_mask = channels_triggering_mask | 12;
    }

    // -------------------------------------------------------------------------
    //  Trigger settings
    // -------------------------------------------------------------------------

    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::Configure() ";
        std::cout << "Setting the timestamp to a continuous increment; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_SetTrigTimeMode(adq_cu_ptr, adq_num, 0));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::Configure() ";
        std::cout << "Setting trigger; ";
        std::cout << "mode: " << ADQ_descriptions::trigger_mode.at(trigger_mode) << "; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_SetTriggerMode(adq_cu_ptr, adq_num, trigger_mode));

    if (trigger_mode == ADQ_EXT_TRIGGER_MODE) {
        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ412::Configure() ";
            std::cout << "Setting external TTL trigger; ";
            std::cout << std::endl;
        }

        CHECKZERO(ADQ_SetExternTrigEdge(adq_cu_ptr, adq_num, trigger_slope));
        //CHECKZERO(ADQ_SetExternalTriggerDelay(adq_cu_ptr, adq_num,  trig_external_delay));
    } else if (trigger_mode == ADQ_LEVEL_TRIGGER_MODE) {
        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ412::Configure() ";
            std::cout << "Setting channels trigger; ";
            std::cout << std::endl;
        }

        CHECKZERO(ADQ_SetLvlTrigLevel(adq_cu_ptr, adq_num, trigger_level));
        CHECKZERO(ADQ_SetLvlTrigEdge(adq_cu_ptr,  adq_num, trigger_slope));
        CHECKZERO(ADQ_SetLvlTrigChannel(adq_cu_ptr, adq_num, channels_triggering_mask));
    }

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::Configure() ";
        std::cout << "Trigger from device: ";
        std::cout << ADQ_descriptions::trigger_mode.at(ADQ_GetTriggerMode(adq_cu_ptr, adq_num)) << "; ";
        std::cout << "Channels triggering mask: " << ADQ_GetLvlTrigChannel(adq_cu_ptr, adq_num) << "; ";
        std::cout << "Level: " << ADQ_GetLvlTrigLevel(adq_cu_ptr, adq_num) << "; ";
        std::cout << "Edge: " << ADQ_descriptions::trigger_slope.at(ADQ_GetLvlTrigEdge(adq_cu_ptr, adq_num)) << "; ";
        std::cout << std::endl;
    }

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

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::Configure() ";
        std::cout << "Setting pretrigger: " << pretrigger << "; ";
        std::cout << std::endl;
    }

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
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::StartAcquisition() ";
        std::cout << "Starting acquisition; ";
        std::cout << "Trigger mode: " << ADQ_descriptions::trigger_mode.at(trigger_mode) << "; ";
        std::cout << std::endl;
    }

    RearmTrigger();

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ412::RearmTrigger()
{
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::RearmTrigger() ";
        std::cout << "Rearming trigger; ";
        std::cout << "Trigger mode: " << ADQ_descriptions::trigger_mode.at(trigger_mode) << " (index: " << trigger_mode << "); ";
        std::cout << std::endl;
    }

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

    const unsigned int retval = ADQ_GetAcquiredAll(adq_cu_ptr, adq_num);

    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::AcquisitionReady() ";
        std::cout << "Acquisition ready: " << retval << "; ";
        std::cout << std::endl;
    }

    return retval == 1 ? true : false;
}

//==============================================================================

bool ABCD::ADQ412::DataOverflow()
{

    const unsigned int retval = ADQ_GetStreamOverflow(adq_cu_ptr, adq_num);

    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::DataOverflow() ";
        std::cout << "Overflow: " << retval << "; ";
        std::cout << std::endl;
    }

    return retval == 0 ? false : true;
}

//==============================================================================

int ABCD::ADQ412::GetWaveformsFromCard(std::vector<struct event_waveform> &waveforms)
{
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
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::GetWaveformsFromCard() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Error in fetching data; ";
        std::cout << std::endl;

        return DIGITIZER_FAILURE;
    } else if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::GetWaveformsFromCard() ";
        std::cout << "Collected all samples; ";
        std::cout << std::endl;
    }

    for (unsigned int record_index = 0; record_index < records_number; record_index++) {
        // If we see a jump backward in timestamp bigger than half the timestamp
        // range we assume that there was an overflow in the timestamp counter.
        // A smaller jump could mean that the records in the buffer are not
        // sorted according to the timestamp, that would make sense but we have
        // not verified it.
        const int64_t timestamp_negative_difference = timestamp_last - target_timestamps[record_index];

        if (timestamp_negative_difference > ADQ412_TIMESTAMP_THRESHOLD) {
            if (GetVerbosity() > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ412::GetWaveformsFromCard() ";
                std::cout << WRITE_YELLOW << "WARNING" << WRITE_NC << ": Detected timestamp overflow; ";
                std::cout << "Overflows: " << timestamp_overflows << "; ";
                std::cout << "Negative difference: " << (long long)timestamp_negative_difference << "; ";
                std::cout << std::endl;
            }
    
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

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::GetWaveformsFromCard() ";
        std::cout << "Converted all samples; ";
        std::cout << "Timestamp overflows: " << timestamp_overflows << "; ";
        std::cout << std::endl;
    }

    //ADQ_GetOverflow(adq_cu_ptr, adq_num);

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ412::StopAcquisition()
{
    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ412::ForceSoftwareTrigger()
{
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::ForceSoftwareTrigger() ";
        std::cout << "Forcing a software trigger; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));
    CHECKZERO(ADQ_ArmTrigger(adq_cu_ptr, adq_num));
    CHECKZERO(ADQ_SWTrig(adq_cu_ptr, adq_num));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ412::ResetOverflow()
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::ResetOverflow() ";
        std::cout << "Resetting a data overflow; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_OVERFLOW));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ412::ReadConfig(json_t *config)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::ReadConfig() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::ReadConfig() ";
        std::cout << "Card is " << (enable ? "enabled" : "disabled") << "; ";
        std::cout << std::endl;
    }

    const char *cstr_clock_source = json_string_value(json_object_get(config, "clock_source"));
    const std::string str_clock_source = (cstr_clock_source) ? std::string(cstr_clock_source) : std::string();

    // Looking for the settings in the description map
    const auto cs_result = map_utilities::find_item(ADQ_descriptions::clock_source, str_clock_source);

    if (cs_result != ADQ_descriptions::clock_source.end() && str_clock_source.length() > 0) {
        clock_source = cs_result->first;
    } else {
        clock_source = ADQ_CLOCK_INT_INTREF;

        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::ReadConfig() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Wrong clock source";
        std::cout << std::endl;
    }

    json_object_set_nocheck(config, "clock_source", json_string(ADQ_descriptions::clock_source.at(clock_source).c_str()));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::ReadConfig() ";
        std::cout << "Clock source: got: " << ADQ_descriptions::clock_source.at(clock_source) << " (index: " << clock_source << "); ";
        std::cout << std::endl;
    }

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

    const char *cstr_trigger_source = json_string_value(json_object_get(trigger_config, "source"));
    const std::string str_trigger_source = (cstr_trigger_source) ? std::string(cstr_trigger_source) : std::string();

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::ReadConfig() ";
        std::cout << "Trigger source: " << str_trigger_source<< "; ";
        std::cout << std::endl;
    }

    // Looking for the settings in the description map
    const auto tm_result = map_utilities::find_item(ADQ_descriptions::trigger_mode, str_trigger_source);

    if (tm_result != ADQ_descriptions::trigger_mode.end() && str_trigger_source.length() > 0) {
        trigger_mode = tm_result->first;
    } else {
        trigger_mode = ADQ_LEVEL_TRIGGER_MODE;

        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::ReadConfig() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Invalid trigger source; ";
        std::cout << "Got: " << str_trigger_source << "; ";
        std::cout << std::endl;
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

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::ReadConfig() ";
        std::cout << "Trigger slope: " << str_trigger_slope << "; ";
        std::cout << std::endl;
    }

    // Looking for the settings in the description map
    const auto ts_result = map_utilities::find_item(ADQ_descriptions::trigger_slope, str_trigger_slope);

    if (ts_result != ADQ_descriptions::trigger_mode.end() && str_trigger_slope.length() > 0) {
        trigger_slope = ts_result->first;
    } else {
        trigger_slope = ADQ_TRIG_SLOPE_FALLING;

        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::ReadConfig() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Invalid trigger slope; ";
        std::cout << "Got: " << str_trigger_slope << "; ";
        std::cout << std::endl;
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

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::ReadConfig() ";
        std::cout << "Trigger level: " << trigger_level << "; ";
        std::cout << std::endl;
    }

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

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ412::ReadConfig() ";
                    std::cout << "Found channel: " << id << "; ";
                    std::cout << std::endl;
                }

                const bool enabled = json_is_true(json_object_get(value, "enable"));

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ412::ReadConfig() ";
                    std::cout << "Channel is " << (enabled ? "enabled" : "disabled") << "; ";
                    std::cout << std::endl;
                }

                json_object_set_new_nocheck(value, "enable", json_boolean(enabled));

                const bool triggering = json_is_true(json_object_get(value, "triggering"));

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ412::ReadConfig() ";
                    std::cout << "Channel is " << (triggering ? "" : "not ") << "triggering; ";
                    std::cout << std::endl;
                }

                json_object_set_new_nocheck(value, "triggering", json_boolean(triggering));


                if (0 <= id && id < static_cast<int>(GetChannelsNumber())) {
                    SetChannelEnabled(id, enabled);
                    SetChannelTriggering(id, triggering);
                } else {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ412::ReadConfig() ";
                    std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Channel out of range, ignoring it; ";
                    std::cout << std::endl;
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    //  Reading the waveforms settings
    // -------------------------------------------------------------------------

    pretrigger = json_integer_value(json_object_get(config, "pretrigger"));

    // TODO: Decide whether to use the external trigger delay or not
    // The documentation says that the card should be able to take care of it.
    // trig_external_delay = pretrigger

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::ReadConfig() ";
        std::cout << "Pretrigger: " << pretrigger << "; ";
        std::cout << std::endl;
    }

    json_object_set_nocheck(config, "pretrigger", json_integer(pretrigger));

    const uint32_t max_samples_per_record = ADQ_GetMaxBufferSize(adq_cu_ptr, adq_num);

    const uint32_t scope_samples = json_integer_value(json_object_get(config, "scope_samples"));

    if (scope_samples < 1 || max_samples_per_record < scope_samples) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::ReadConfig() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Samples number out of range";
        std::cout << std::endl;

        if (scope_samples < 1) {
            samples_per_record = 1;
        } else if (max_samples_per_record < scope_samples) {
            samples_per_record = max_samples_per_record;
        }
    } else {
        samples_per_record = scope_samples;
    }

    json_object_set_nocheck(config, "scope_samples", json_integer(samples_per_record));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::ReadConfig() ";
        std::cout << "Scope samples: got: " << scope_samples << "; ";
        std::cout << std::endl;
    }

    unsigned int max_records_number = 0;
    CHECKZERO(ADQ_GetMaxNofRecordsFromNofSamples(adq_cu_ptr, adq_num,
                                                 samples_per_record,
                                                 &max_records_number));
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::ReadConfig() ";
        std::cout << "Maximum records number: " << max_records_number << " (for scope_samples: " << samples_per_record << "); ";
        std::cout << std::endl;
    }

    records_number = json_integer_value(json_object_get(config, "records_number"));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::ReadConfig() ";
        std::cout << "Number of records: got: " << records_number << "; ";
        std::cout << std::endl;
    }

    if (records_number < 1 || max_records_number < records_number) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::ReadConfig() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Records number out of range";
        std::cout << std::endl;

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
    const char *cstr_command = json_string_value(json_object_get(json_command, "command"));
    const std::string command = (cstr_command) ? std::string(cstr_command) : std::string();

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ412::SpecificCommand() ";
        std::cout << "Specific command: " << command << "; ";
        std::cout << std::endl;
    }

    if (command == std::string("GPIO_set_direction")) {
        const char *cstr_direction = json_string_value(json_object_get(json_command, "direction"));
        const std::string direction = (cstr_direction) ? std::string(cstr_direction) : std::string();

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ412::SpecificCommand() ";
            std::cout << "Setting GPIO direction: " << direction << "; ";
            std::cout << std::endl;
        }

        // Informing that all the pins should keep their direction, only pin 5
        // can be changed in these boards.
        const uint16_t mask = 0xF;
        uint16_t pins_directions = 0;

        if (direction == std::string("input")) {
            pins_directions = 0;
        } else if (direction == std::string("output")) {
            pins_directions = (1 << 4);
        }

        CHECKZERO(ADQ_SetDirectionGPIO(adq_cu_ptr, adq_num, pins_directions, mask));
    } else if (command == std::string("GPIO_write")) {
        const int value = json_integer_value(json_object_get(json_command, "value"));

        // Informing that all the pins should ignore the command, only pin 5 can
        // be used in these boards.
        const uint16_t mask = 0xF;
        const uint16_t pin_value = (value > 0) ? (1 << 4) : 0;

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ412::SpecificCommand() ";
            std::cout << "Writing GPIO: " << value << ", pin_value: " << (unsigned int)pin_value << "; ";
            std::cout << std::endl;
        }

        CHECKZERO(ADQ_WriteGPIO(adq_cu_ptr, adq_num, pin_value, mask));
    } else if (command == std::string("GPIO_pulse")) {
        const int width = json_integer_value(json_object_get(json_command, "width"));

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ412::SpecificCommand() ";
            std::cout << "Pulse on GPIO of width: " << width << " us; ";
            std::cout << std::endl;
        }

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

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ412::SpecificCommand() ";
            std::cout << "Timestamp reset mode: " << mode << "; ";
            std::cout << std::endl;
        }

        int restart_mode = 1;

        if (mode == std::string("pulse")) {
            restart_mode = 0;
        } else if (mode == std::string("now")) {
            restart_mode = 1;
        }

        CHECKZERO(ADQ_ResetTrigTimer(adq_cu_ptr, adq_num, restart_mode));
    } else if (command == std::string("GPIO_read")) {
        const int read_pins_values = ADQ_ReadGPIO(adq_cu_ptr, adq_num);

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ412::SpecificCommand() ";
            std::cout << "Read GPIO values: " << read_pins_values << "; ";
            std::cout << std::endl;
        }
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================
