#include <cstdlib>
#include <cinttypes>

#include <fstream>
#include <iostream>
#include <iomanip>
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
#include "ADQ214.hpp"

#define BUFFER_SIZE 32

const int ABCD::ADQ214::default_trigger_output_width = 20;

ABCD::ADQ214::ADQ214(void* adq, int num, int Verbosity) : ABCD::Digitizer(Verbosity),
                                                          adq_cu_ptr(adq),
                                                          adq_num(num)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ADQ214() ";
        std::cout << std::endl;
    }

    SetModel("ADQ214");

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
}

//==============================================================================

ABCD::ADQ214::~ADQ214() { }

//==============================================================================

int ABCD::ADQ214::Initialize()
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::Initialize() ";
        std::cout << std::endl;
    }

    // The vendor says that these should be used only for USB digitizers
    //CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_POWER_ON));
    //CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_COMMUNICATION));

    const char *board_name = ADQ_GetBoardSerialNumber(adq_cu_ptr, adq_num);

    if (!board_name) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::Initialize() ";
        std::cout << "Initialized board; ";
        std::cout << std::endl;
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::Initialize() ";
        std::cout << "Card name (serial number): " << GetName() << "; ";
        std::cout << "Product name: " << ADQ_GetBoardProductName(adq_cu_ptr, adq_num) << "; ";
        std::cout << std::endl;

        std::cout << '[' << time_buffer << "] ABCD::ADQ214::Initialize() ";
        std::cout << "USB address: " << ADQ_GetUSBAddress(adq_cu_ptr, adq_num) << "; ";
        std::cout << "PCIe address: " << ADQ_GetPCIeAddress(adq_cu_ptr, adq_num) << "; ";
        std::cout << std::endl;

        std::cout << '[' << time_buffer << "] ABCD::ADQ214::Initialize() ";
        std::cout << "ADQAPI Revision: " << ADQAPI_GetRevisionString() << "; ";
        std::cout << "ADQ214 Revision: {";
        uint32_t* revision = ADQ_GetRevision(adq_cu_ptr, adq_num);
        for (int i = 0; i < 6; i++) {
            std::cout << (unsigned int)revision[i] << ", ";
        }
        std::cout << "}; ";
        std::cout << std::endl;

        std::cout << '[' << time_buffer << "] ABCD::ADQ214::Initialize() ";
        std::cout << "Channels number: " << GetChannelsNumber() << "; ";
        std::cout << "DBS instances: " << dbs_inst << "; ";
        std::cout << std::endl;

        std::cout << '[' << time_buffer << "] ABCD::ADQ214::Initialize() ";
        std::cout << "Has adjustable input range: " << (ADQ_HasAdjustableInputRange(adq_cu_ptr, adq_num) > 0 ? "true" : "false") << "; ";
        std::cout << "Has adjustable offset: " << (ADQ_HasAdjustableBias(adq_cu_ptr, adq_num) > 0 ? "true" : "false") << "; ";
        std::cout << std::endl;

        for (auto &pair : ADQ_descriptions::ADQ214_temperatures) {
            const double temperature = ADQ_GetTemperature(adq_cu_ptr, adq_num, pair.first) / 256.0;

            std::cout << '[' << time_buffer << "] ABCD::ADQ214::Initialize() ";
            std::cout << pair.second << " temperature: " << temperature << "; ";
            std::cout << std::endl;
        }
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ214::Configure()
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::Configure() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::Configure() ";
        std::cout << "Setting PLL divider; ";
        std::cout << "PLL_divider: " << PLL_divider << "; ";
        std::cout << std::endl;
    }

    // This has to go before the SetClockSource
    CHECKZERO(ADQ_SetPllFreqDivider(adq_cu_ptr, adq_num, PLL_divider));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::Configure() ";
        std::cout << "Setting clock; ";
        std::cout << "clock_source: " << ADQ_descriptions::clock_source.at(clock_source) << "; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_SetClockSource(adq_cu_ptr, adq_num, clock_source));

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::Configure() ";
        std::cout << "Clock source from device: " << ADQ_descriptions::clock_source.at(ADQ_GetClockSource(adq_cu_ptr, adq_num)) << "; ";
        std::cout << std::endl;
    }

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::Configure() ";
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
    channels_analog_front_end_mask = 0;

    for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++) {
        int gain;
        int offset;

        // Channel indexing starts from 1
        CHECKZERO(ADQ_GetGainAndOffset(adq_cu_ptr, adq_num, channel + 1, &gain, &offset));

        // This is just a value added to the ADC conversion and not a physical offset
        //CHECKZERO(ADQ_SetGainAndOffset(adq_cu_ptr, adq_num, channel + 1, gain, 0));

        // This is not supported in the ADQ214
        //CHECKZERO(ADQ_SetAdjustableBias(adq_cu_ptr, adq_num, channel + 1, 30000));

        uint8_t analog_front_end_mode;

        CHECKZERO(ADQ_GetAfeSwitch(adq_cu_ptr, adq_num, channel + 1, &analog_front_end_mode));

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ214::Configure() ";
            std::cout << "Channel: " << channel << "; ";
            std::cout << "Enabled: " << (IsChannelEnabled(channel) ? "true" : "false") << "; ";
            std::cout << "Triggering: " << (IsChannelTriggering(channel) ? "true" : "false") << "; ";
            std::cout << "Gain: " << gain << "; ";
            std::cout << "Offset: " << offset << "; ";
            std::cout << "Analog front end mode: " << (unsigned int)analog_front_end_mode << "; ";
            std::cout << std::endl;
        }

        if (IsChannelTriggering(channel)) {
            channels_triggering_mask += (1 << channel);
        }
        if (IsChannelEnabled(channel)) {
            channels_acquisition_mask += (1 << channel);
        }
        if (analog_front_end_couplings[channel] == ADQ_ANALOG_FRONT_END_COUPLING_DC) {
            channels_analog_front_end_mask += (1 << channel);
        }

        // Enabling LF amplification on the channels
        // TODO Discover what this means
        channels_analog_front_end_mask += (1 << (channel + 2));
    }

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::Configure() ";
        std::cout << "Channels acquisition mask: " << (unsigned int)channels_acquisition_mask << "; ";
        std::cout << "Channels triggering mask: " << channels_triggering_mask << "; ";
        std::cout << "Channels analong front end mask: " << (unsigned int)channels_analog_front_end_mask << "; ";
        std::cout << std::endl;
    }

    // Only a set of possible masks are allowed, if the mask is wrong we use
    // the most conservative option of enabling all channels.
    if (!(channels_triggering_mask == 0 || // No channel, probably external trigger
          channels_triggering_mask == 1 || // Channel 0
          channels_triggering_mask == 2 || // Channel 1
          channels_triggering_mask == 3 || // Channel 0 and 1
          channels_triggering_mask == 15)) // All channels
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::Configure() ";
        std::cout << WRITE_YELLOW << "WARNING" << WRITE_NC << ": Wrong triggering mask (got: " << channels_triggering_mask << "), enabling all channels; ";
        std::cout << std::endl;

        channels_triggering_mask = 15;
    }

    CHECKZERO(ADQ_SetAfeSwitch(adq_cu_ptr, adq_num, channels_analog_front_end_mask));

    // -------------------------------------------------------------------------
    //  Trigger settings
    // -------------------------------------------------------------------------

    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::Configure() ";
        std::cout << "Setting trigger; ";
        std::cout << "mode: " << ADQ_descriptions::trigger_mode.at(trigger_mode) << "; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_SetTriggerMode(adq_cu_ptr, adq_num, trigger_mode));

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::Configure() ";
        std::cout << "Trigger from device: ";
        std::cout << ADQ_descriptions::trigger_mode.at(ADQ_GetTriggerMode(adq_cu_ptr, adq_num)) << "; ";
        std::cout << std::endl;
    }

    if (trigger_mode == ADQ_EXT_TRIGGER_MODE) {
        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ214::Configure() ";
            std::cout << "Setting external TTL trigger; ";
            std::cout << std::endl;
        }

        CHECKZERO(ADQ_SetExternTrigEdge(adq_cu_ptr, adq_num, trigger_slope));
        //CHECKZERO(ADQ_SetExternalTriggerDelay(adq_cu_ptr, adq_num,  trigger_external_delay));
    } else if (trigger_mode == ADQ_LEVEL_TRIGGER_MODE) {
        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ214::Configure() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::Configure() ";
        std::cout << "Trigger from device: ";
        std::cout << ADQ_descriptions::trigger_mode.at(ADQ_GetTriggerMode(adq_cu_ptr, adq_num)) << "; ";
        std::cout << "Channels triggering mask: " << ADQ_GetLvlTrigChannel(adq_cu_ptr, adq_num) << "; ";
        std::cout << "Level: " << ADQ_GetLvlTrigLevel(adq_cu_ptr, adq_num) << "; ";
        std::cout << "Edge: " << ADQ_descriptions::trigger_slope.at(ADQ_GetLvlTrigEdge(adq_cu_ptr, adq_num)) << "; ";
        std::cout << std::endl;
    }

    // -------------------------------------------------------------------------
    //  Trigger output
    // -------------------------------------------------------------------------

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::Configure() ";
        std::cout << "Trigger output: " << (trigger_output_enable ? "true" : "false") << "; ";
        std::cout << "mode: " <<                                               (trigger_output_mode) << "; ";
        std::cout << "width: " << trigger_output_width << "; ";
        std::cout << std::endl;
    }

    if (trigger_output_enable) {
        // WriteTrig() needs to be set to 1, in order to enable the trigger output
        CHECKZERO(ADQ_WriteTrig(adq_cu_ptr, adq_num, 1));
        CHECKZERO(ADQ_SetConfigurationTrig(adq_cu_ptr, adq_num, trigger_output_mode,
                                                                trigger_output_width, 0));

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ214::Configure() ";
            std::cout << "Trigger output: enabled; ";
            std::cout << std::endl;
        }
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
    CHECKZERO(ADQ_SetDataFormat(adq_cu_ptr, adq_num, ADQ214_DATA_FORMAT_PACKED_14BIT));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::Configure() ";
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
    target_headers.resize(records_number * ADQ214_RECORD_HEADER_SIZE / sizeof(uint32_t));

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
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::SetChannelsNumber() ";
        std::cout << "Setting channels number to: " << n << "; ";
        std::cout << std::endl;
    }

    Digitizer::SetChannelsNumber(n);

    analog_front_end_couplings.resize(n, ADQ_ANALOG_FRONT_END_COUPLING_DC);
}

//==============================================================================

int ABCD::ADQ214::StartAcquisition()
{
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::StartAcquisition() ";
        std::cout << "Starting acquisition; ";
        std::cout << "Trigger mode: " << ADQ_descriptions::trigger_mode.at(trigger_mode) << "; ";
        std::cout << std::endl;
    }

    RearmTrigger();

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ214::RearmTrigger()
{
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::RearmTrigger() ";
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

bool ABCD::ADQ214::AcquisitionReady()
{

    const unsigned int retval = ADQ_GetAcquiredAll(adq_cu_ptr, adq_num);

    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::AcquisitionReady() ";
        std::cout << "Acquisition ready: " << retval << "; ";
        std::cout << std::endl;
    }

    return retval == 1 ? true : false;
}

//==============================================================================

bool ABCD::ADQ214::DataOverflow()
{

    const unsigned int retval = ADQ_GetStreamOverflow(adq_cu_ptr, adq_num);

    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::DataOverflow() ";
        std::cout << "Overflow: " << retval << "; ";
        std::cout << std::endl;
    }

    return retval == 0 ? false : true;
}

//==============================================================================

int ABCD::ADQ214::GetWaveformsFromCard(std::vector<struct event_waveform> &waveforms)
{
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

    if (retval == 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::GetWaveformsFromCard() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Error in fetching data; ";
        std::cout << std::endl;

        return DIGITIZER_FAILURE;
    } else if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::GetWaveformsFromCard() ";
        std::cout << "Collected all samples; ";
        std::cout << std::endl;
    }

    for (unsigned int record_index = 0; record_index < records_number; record_index++) {
	const size_t header_start = record_index * (ADQ214_RECORD_HEADER_SIZE / sizeof(uint32_t));

        if (GetVerbosity() > 2)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ214::GetWaveformsFromCard() ";
            std::cout << "Record header words: ";

	    for (unsigned int i = 0; i < (ADQ214_RECORD_HEADER_SIZE / sizeof(uint32_t)); i++)
            {
	        const uint32_t word = target_headers[header_start + i];
                std::cout << "0x" << std::setfill('0') << std::setw(8) << std::hex << word << std::dec << " ";
            }

            std::cout << "; " << std::endl;

            std::cout << '[' << time_buffer << "] ABCD::ADQ214::GetWaveformsFromCard() ";
            std::cout << "Sub-sample time resolution bits: " << "0x" << std::setw(1) << std::hex << (target_headers[header_start + 2] & ADQ214_EXTENDED_TIMESTAMP_BITMASK) << std::dec << "; ";
            std::cout << "Last timestamp bits: " << "0x" << std::setw(1) << std::hex << (target_timestamps[record_index] & 0b11) << std::dec << "; ";
            std::cout << std::endl;
	}

        // If we see a jump backward in timestamp bigger than half the timestamp
        // range we assume that there was an overflow in the timestamp counter.
        // A smaller jump could mean that the records in the buffer are not
        // sorted according to the timestamp, that would make sense but we have
        // not verified it.
        const int64_t timestamp_negative_difference = timestamp_last - target_timestamps[record_index];

        if (timestamp_negative_difference > ADQ214_TIMESTAMP_THRESHOLD) {
            if (GetVerbosity() > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ214::GetWaveformsFromCard() ";
                std::cout << WRITE_YELLOW << "WARNING" << WRITE_NC << ": Detected timestamp overflow; ";
                std::cout << "Overflows: " << timestamp_overflows << "; ";
                std::cout << "Negative difference: " << (long long)timestamp_negative_difference << "; ";
                std::cout << std::endl;
            }
    
            timestamp_offset += ADQ214_TIMESTAMP_MAX;
            timestamp_overflows += 1;
        }

        // We do not add the offset here because we want to check the digitizer
        // behaviour and not the correction.
        timestamp_last = target_timestamps[record_index] + (target_headers[header_start + 2] & ADQ214_EXTENDED_TIMESTAMP_BITMASK);

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
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::GetWaveformsFromCard() ";
        std::cout << "Converted all samples; ";
        std::cout << "Timestamp overflows: " << timestamp_overflows << "; ";
        std::cout << std::endl;
    }

    //ADQ_GetOverflow(adq_cu_ptr, adq_num);

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ214::StopAcquisition()
{
    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ214::ForceSoftwareTrigger()
{
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ForceSoftwareTrigger() ";
        std::cout << "Forcing a software trigger; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));
    CHECKZERO(ADQ_ArmTrigger(adq_cu_ptr, adq_num));
    CHECKZERO(ADQ_SWTrig(adq_cu_ptr, adq_num));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ214::ResetOverflow()
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ResetOverflow() ";
        std::cout << "Resetting a data overflow; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_OVERFLOW));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ214::ReadConfig(json_t *config)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Wrong clock source";
        std::cout << std::endl;
    }

    json_object_set_nocheck(config, "clock_source", json_string(ADQ_descriptions::clock_source.at(clock_source).c_str()));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
        std::cout << "Clock source: got: " << ADQ_descriptions::clock_source.at(clock_source) << " (index: " << clock_source << "); ";
        std::cout << std::endl;
    }

    const int raw_PLL_divider = json_integer_value(json_object_get(config, "PLL_divider"));

    if (2 <= raw_PLL_divider && raw_PLL_divider <= 20) {
        PLL_divider = raw_PLL_divider;
    } else {
        PLL_divider = 2;

        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Invalid PLL divider value; ";
        std::cout << "Got: " << raw_PLL_divider << "; ";
        std::cout << std::endl;
    }

    json_object_set_nocheck(config, "PLL_divider", json_integer(PLL_divider));


    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
        std::cout << "PLL divider: " << PLL_divider << "; ";
        std::cout << std::endl;
    }

    // -------------------------------------------------------------------------
    //  Reading the trigger configuration
    // -------------------------------------------------------------------------
    json_t *trigger_config = json_object_get(config, "trigger");

    const char *cstr_trigger_source = json_string_value(json_object_get(trigger_config, "source"));
    const std::string str_trigger_source = (cstr_trigger_source) ? std::string(cstr_trigger_source) : std::string();

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Invalid trigger slope; ";
        std::cout << "Got: " << str_trigger_slope << "; ";
        std::cout << std::endl;
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

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
        std::cout << "Trigger level: " << trigger_level << "; ";
        std::cout << std::endl;
    }

    // -------------------------------------------------------------------------
    //  Reading the trigger output configuration
    // -------------------------------------------------------------------------
    json_t *trigger_output_config = json_object_get(config, "trigger_output");

    if (!json_is_object(trigger_output_config)) {
        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ214::Configure() ";
            std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Missing \"trigger_output\" entry in configuration; ";
            std::cout << std::endl;
        }

        trigger_output_config = json_object();
        json_object_set_new_nocheck(config, "trigger_output", trigger_output_config);
    }

    trigger_output_enable = json_is_true(json_object_get(trigger_output_config, "enable"));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
        std::cout << "Trigger output is " << (trigger_output_enable ? "enabled" : "disabled") << "; ";
        std::cout << std::endl;
    }

    json_object_set_nocheck(trigger_output_config, "enable", trigger_output_enable ? json_true() : json_false());

    const char *cstr_trigger_output_source = json_string_value(json_object_get(trigger_output_config, "source"));
    const std::string str_trigger_output_source = (cstr_trigger_output_source) ? std::string(cstr_trigger_output_source) : std::string();

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
        std::cout << "Trigger output source: " << str_trigger_output_source<< "; ";
        std::cout << std::endl;
    }

    // Looking for the settings in the description map
    const auto tom_result = map_utilities::find_item(ADQ_descriptions::ADQ214_trigger_output_mode, str_trigger_output_source);

    if (tom_result != ADQ_descriptions::ADQ214_trigger_output_mode.end() && str_trigger_output_source.length() > 0) {
        trigger_output_mode = tom_result->first;
    } else {
        trigger_output_mode = ADQ_ADQ214_TRIGGER_OUTPUT_MODE_DISABLE;

        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Invalid trigger_output source; ";
        std::cout << "Got: " << str_trigger_output_source << "; ";
        std::cout << std::endl;
    }

    if (!trigger_output_enable) {
        if (GetVerbosity() > 0) {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
            std::cout << "Trigger output: forcing disable; ";
            std::cout << std::endl;
        }

        trigger_output_mode = ADQ_ADQ214_TRIGGER_OUTPUT_MODE_DISABLE;
    }

    json_object_set_nocheck(trigger_output_config, "source", json_string(ADQ_descriptions::ADQ214_trigger_output_mode.at(trigger_output_mode).c_str()));

    trigger_output_width = json_number_value(json_object_get(trigger_output_config, "width"));

    if (trigger_output_width < ABCD::ADQ214::default_trigger_output_width) {
       trigger_output_width = ABCD::ADQ214::default_trigger_output_width;
    }

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
        std::cout << "Trigger output width: " << trigger_output_width << " ns; ";
        std::cout << std::endl;
    }

    json_object_set_nocheck(trigger_output_config, "width", json_integer(trigger_output_width));

    // -------------------------------------------------------------------------
    //  Reading the single channels configuration
    // -------------------------------------------------------------------------
    // First resetting the channels statuses
    for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++) {
        SetChannelEnabled(channel, false);
        SetChannelTriggering(channel, false);
        analog_front_end_couplings[channel] = ADQ_ANALOG_FRONT_END_COUPLING_DC;
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
                    std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
                    std::cout << "Found channel: " << id << "; ";
                    std::cout << std::endl;
                }

                const bool enabled = json_is_true(json_object_get(value, "enable"));

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
                    std::cout << "Channel is " << (enabled ? "enabled" : "disabled") << "; ";
                    std::cout << std::endl;
                }

                json_object_set_new_nocheck(value, "enable", json_boolean(enabled));

                const bool triggering = json_is_true(json_object_get(value, "triggering"));

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
                    std::cout << "Channel is " << (triggering ? "" : "not ") << "triggering; ";
                    std::cout << std::endl;
                }

                json_object_set_new_nocheck(value, "triggering", json_boolean(triggering));

                const char *cstr_coupling = json_string_value(json_object_get(value, "coupling"));
                const std::string str_coupling = (cstr_coupling) ? std::string(cstr_coupling) : std::string();

                if (GetVerbosity() > 0) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
                    std::cout << "Channel coupling: " << str_coupling << "; ";
                    std::cout << std::endl;
                }

                // Looking for the settings in the description map
                const auto cp_result = map_utilities::find_item(ADQ_descriptions::analog_front_end_coupling, str_coupling);

                unsigned int analog_front_end_coupling = ADQ_ANALOG_FRONT_END_COUPLING_DC;

                if (cp_result != ADQ_descriptions::analog_front_end_coupling.end() && str_coupling.length() > 0) {
                    analog_front_end_coupling = cp_result->first;
                } else {
                    analog_front_end_coupling = ADQ_ANALOG_FRONT_END_COUPLING_DC;

                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
                    std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Invalid coupling; ";
                    std::cout << "Got: " << str_coupling << "; ";
                    std::cout << std::endl;
                }

                json_object_set_nocheck(value, "coupling", json_string(ADQ_descriptions::analog_front_end_coupling.at(analog_front_end_coupling).c_str()));

                if (0 <= id && id < static_cast<int>(GetChannelsNumber())) {
                    SetChannelEnabled(id, enabled);
                    SetChannelTriggering(id, triggering);
                    analog_front_end_couplings[id] = analog_front_end_coupling;
                } else {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
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
    // trigger_external_delay = pretrigger

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
        std::cout << "Pretrigger: " << pretrigger << "; ";
        std::cout << std::endl;
    }

    json_object_set_nocheck(config, "pretrigger", json_integer(pretrigger));

    const uint32_t max_samples_per_record = ADQ_GetMaxBufferSize(adq_cu_ptr, adq_num);

    const uint32_t scope_samples = json_integer_value(json_object_get(config, "scope_samples"));

    if (scope_samples < 1 || max_samples_per_record < scope_samples) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
        std::cout << "Maximum records number: " << max_records_number << " (for scope_samples: " << samples_per_record << "); ";
        std::cout << std::endl;
    }

    records_number = json_integer_value(json_object_get(config, "records_number"));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
        std::cout << "Number of records: got: " << records_number << "; ";
        std::cout << std::endl;
    }

    if (records_number < 1 || max_records_number < records_number) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::ReadConfig() ";
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

int ABCD::ADQ214::SpecificCommand(json_t *json_command)
{
    const char *cstr_command = json_string_value(json_object_get(json_command, "command"));
    const std::string command = (cstr_command) ? std::string(cstr_command) : std::string();

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::SpecificCommand() ";
        std::cout << "Specific command: " << command << "; ";
        std::cout << std::endl;
    }

    if (command == std::string("GPIO_set_direction")) {
        const unsigned int pin = json_number_value(json_object_get(json_command, "pin"));

        const char *cstr_direction = json_string_value(json_object_get(json_command, "direction"));
        const std::string direction = (cstr_direction) ? std::string(cstr_direction) : std::string();

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ214::SpecificCommand() ";
            std::cout << "Setting GPIO pin: " << pin << "; direction: " << direction << "; ";
            std::cout << std::endl;
        }

        // Mask has an inverted logic
        const uint16_t mask = ~(1 << pin);
        uint16_t pins_directions = 0;

        if (direction == std::string("input")) {
            pins_directions = 0;
        } else if (direction == std::string("output")) {
            pins_directions = (1 << pin);
        }

        GPIOSetDirection(pins_directions, mask);
    } else if (command == std::string("GPIO_write")) {
        const unsigned int pin = json_number_value(json_object_get(json_command, "pin"));
        const unsigned int value = json_number_value(json_object_get(json_command, "value"));

        // Mask has an inverted logic
        const uint16_t mask = ~(1 << pin);
        const uint16_t pins_values = (value > 0) ? (1 << pin) : 0;

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ214::SpecificCommand() ";
            std::cout << "Writing GPIO: pin: " << pin << "; value: " << value << ", pins_values: " << (unsigned int)pins_values << "; ";
            std::cout << std::endl;
        }

        GPIOWrite(pins_values, mask);
    } else if (command == std::string("GPIO_pulse")) {
        const unsigned int pin = json_number_value(json_object_get(json_command, "pin"));
        const int width = json_number_value(json_object_get(json_command, "width"));

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ214::SpecificCommand() ";
            std::cout << "Pulse on GPIO: pin: " << pin << " of width: " << width << " us; ";
            std::cout << std::endl;
        }

        // Mask has an inverted logic
        const uint16_t mask = ~(1 << pin);

        GPIOPulse(width, mask);
    } else if (command == std::string("timestamp_reset_mode")) {
        const char *cstr_mode = json_string_value(json_object_get(json_command, "mode"));
        const std::string mode = (cstr_mode) ? std::string(cstr_mode) : std::string();

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ214::SpecificCommand() ";
            std::cout << "Timestamp reset mode: " << mode << "; ";
            std::cout << std::endl;
        }

        TimestampResetMode(mode);
    } else if (command == std::string("GPIO_read")) {
        const int read_pins_values = ADQ_ReadGPIO(adq_cu_ptr, adq_num);

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ214::SpecificCommand() ";
            std::cout << "Read GPIO values: " << read_pins_values << "; ";
            std::cout << std::endl;
        }
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ214::GPIOSetDirection(unsigned int pins_directions, unsigned int mask)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::GPIOSetDirection() ";
        std::cout << "Pins directions: " << std::hex << pins_directions << std::dec << "; ";
        std::cout << "Mask: " << std::hex << mask << std::dec << "; ";
        std::cout << std::endl;
    }

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

int ABCD::ADQ214::GPIOWrite(unsigned int pins_values, unsigned int mask)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::GPIOPulse() ";
        std::cout << "Pins values: " << std::hex << pins_values << std::dec << "; ";
        std::cout << "Mask: " << std::hex << mask << std::dec << "; ";
        std::cout << std::endl;
    }

    // Check notes of function GPIOSetDirection()
    const uint16_t safe_mask = mask | 0xFFE0;

    // FIXME: Check the return value
    ADQ_WriteGPIO(adq_cu_ptr, adq_num, pins_values, safe_mask);

    return DIGITIZER_SUCCESS;
}

int ABCD::ADQ214::GPIOPulse(unsigned int width, unsigned int mask)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::GPIOPulse() ";
        std::cout << "Width: " << width << " us; ";
        std::cout << "Mask: " << std::hex << mask << std::dec << "; ";
        std::cout << std::endl;
    }

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

int ABCD::ADQ214::TimestampResetMode(std::string mode)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ214::TimestampReset() ";
        std::cout << "Timestamp reset mode: " << mode << "; ";
        std::cout << std::endl;
    }

    int restart_mode = 1;

    if (mode == std::string("pulse")) {
        restart_mode = 0;
    } else if (mode == std::string("now")) {
        restart_mode = 1;
    }

    const int return_value = ADQ_ResetTrigTimer(adq_cu_ptr, adq_num, restart_mode);

    if (!return_value) {
        return DIGITIZER_FAILURE;
    } else {
        return DIGITIZER_SUCCESS;
    }
}

//==============================================================================
