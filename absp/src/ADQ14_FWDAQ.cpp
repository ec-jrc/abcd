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
}

#include "map_utilities.hpp"

#define LINUX
#include "ADQAPI.h"
#include "ADQ_descriptions.hpp"
#include "ADQ_utilities.hpp"
#include "ADQ14_FWDAQ.hpp"

#define BUFFER_SIZE 32

// Defined in mVpp
const float ABCD::ADQ14_FWDAQ::default_input_range = 1000;
// Defined in ADC samples
const int ABCD::ADQ14_FWDAQ::default_DC_offset = 0;
// Defined in ADC samples
const int ABCD::ADQ14_FWDAQ::default_DBS_target = 0;
// If left at zero the FWDAQ will use its default values
const int ABCD::ADQ14_FWDAQ::default_DBS_saturation_level_lower = 0;
const int ABCD::ADQ14_FWDAQ::default_DBS_saturation_level_upper = 0;

ABCD::ADQ14_FWDAQ::ADQ14_FWDAQ(void* adq, int num, int Verbosity) : ABCD::Digitizer(Verbosity),
                                                                    adq_cu_ptr(adq),
                                                                    adq_num(num)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ADQ14_FWDAQ() ";
        std::cout << std::endl;
    }

    SetModel("ADQ14_FWDAQ");

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
}

//==============================================================================

ABCD::ADQ14_FWDAQ::~ADQ14_FWDAQ() {
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::Initialize()
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Initialize() ";
        std::cout << std::endl;
    }

    // The vendor says that these should be used only for USB digitizers
    //CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_POWER_ON));
    //CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_COMMUNICATION));
    //CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_ADC_DATA));

    const char *board_name = ADQ_GetBoardSerialNumber(adq_cu_ptr, adq_num);

    if (!board_name) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Error in getting the serial number; ";
        std::cout << std::endl;

        SetName("");
    } else {
        SetName(board_name);
    }

    const unsigned int number_of_channels = ADQ_GetNofChannels(adq_cu_ptr, adq_num);

    SetChannelsNumber(number_of_channels);

    unsigned int adc_cores = 0;
    CHECKZERO(ADQ_GetNofAdcCores(adq_cu_ptr, adq_num, &adc_cores));

    unsigned int dbs_inst = 0;
    CHECKZERO(ADQ_GetNofDBSInstances(adq_cu_ptr, adq_num, &dbs_inst));

    SetDBSInstancesNumber(dbs_inst);

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Initialize() ";
        std::cout << "Initialized board; ";
        std::cout << std::endl;
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Initialize() ";
        std::cout << "Card name (serial number): " << GetName() << "; ";
        std::cout << "Product name: " << ADQ_GetBoardProductName(adq_cu_ptr, adq_num) << "; ";
        std::cout << "Card option: " << ADQ_GetCardOption(adq_cu_ptr, adq_num) << "; ";
        std::cout << std::endl;

        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Initialize() ";
        std::cout << "USB address: " << ADQ_GetUSBAddress(adq_cu_ptr, adq_num) << "; ";
        std::cout << "PCIe address: " << ADQ_GetPCIeAddress(adq_cu_ptr, adq_num) << "; ";
        std::cout << std::endl;

        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Initialize() ";
        std::cout << "ADQAPI Revision: " << ADQAPI_GetRevisionString() << "; ";
        std::cout << "ADQ14 Revision: {";
        uint32_t* revision = ADQ_GetRevision(adq_cu_ptr, adq_num);
        for (int i = 0; i < 6; i++) {
            std::cout << (unsigned int)revision[i] << ", ";
        }
        std::cout << "}; ";
        std::cout << std::endl;

        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Initialize() ";
        std::cout << "Channels number: " << GetChannelsNumber() << "; ";
        std::cout << "ADC cores: " << adc_cores << "; ";
        std::cout << "DBS instances: " << GetDBSInstancesNumber() << "; ";
        std::cout << std::endl;

        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Initialize() ";
        std::cout << "Has adjustable input range: " << (ADQ_HasAdjustableInputRange(adq_cu_ptr, adq_num) > 0 ? "true" : "false") << "; ";
        std::cout << "Has adjustable offset: " << (ADQ_HasAdjustableBias(adq_cu_ptr, adq_num) > 0 ? "true" : "false") << "; ";
        // FIXME: Find out what trigger_num is to be used
        //std::cout << "Has adjustable external trigger threshold: " << (ADQ_HasVariableTrigThreshold(adq_cu_ptr, adq_num) > 0 ? "true" : "false") << "; ";
        std::cout << std::endl;

        for (auto &pair : ADQ_descriptions::ADQ14_temperatures) {
            const double temperature = ADQ_GetTemperature(adq_cu_ptr, adq_num, pair.first) / 256.0;

            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Initialize() ";
            std::cout << pair.second << " temperature: " << temperature << "; ";
            std::cout << std::endl;
        }
    }

    CHECKZERO(ADQ_SetOvervoltageProtection(adq_cu_ptr, adq_num, ADQ_OVERVOLTAGE_PROTECTION_ENABLE));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::Configure()
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Configure() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Configure() ";
        std::cout << "Setting clock; ";
        std::cout << "clock_source: " << ADQ_descriptions::clock_source.at(clock_source) << "; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_SetClockSource(adq_cu_ptr, adq_num, clock_source));

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Configure() ";
        std::cout << "Clock source from device: " << ADQ_GetClockSource(adq_cu_ptr, adq_num) << "; ";
        try {
            std::cout << ADQ_descriptions::clock_source.at(ADQ_GetClockSource(adq_cu_ptr, adq_num)) << "; ";
        } catch (...) {
        }
        std::cout << std::endl;
    }

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Configure() ";
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
        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Configure() ";
            std::cout << "Channel: " << channel << "; ";
            std::cout << "Enabled: " << (IsChannelEnabled(channel) ? "true" : "false") << "; ";
            std::cout << "Triggering: " << (IsChannelTriggering(channel) ? "true" : "false") << "; ";
            std::cout << std::endl;
        }

        if (IsChannelTriggering(channel)) {
            channels_triggering_mask += (1 << channel);
        }
        if (IsChannelEnabled(channel)) {
            channels_acquisition_mask += (1 << channel);
        }

        if (ADQ_HasAdjustableInputRange(adq_cu_ptr, adq_num) > 0) {
            if (GetVerbosity() > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Configure() ";
                std::cout << "Setting input range; ";
                std::cout << std::endl;
            }

            const float desired = desired_input_ranges[channel];
            float result = 0;

            CHECKZERO(ADQ_SetInputRange(adq_cu_ptr, adq_num, channel + 1, desired, &result));

            if (GetVerbosity() > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Configure() ";
                std::cout << "Input range: desired: " << desired << " mVpp, result: " << result << " mVpp; ";
                std::cout << std::endl;
            }
        }

        if (ADQ_HasAdjustableBias(adq_cu_ptr, adq_num) > 0) {
            const int DC_offset = DC_offsets[channel];

            if (GetVerbosity() > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Configure() ";
                std::cout << "Setting DC offset to: " << DC_offset << " samples; ";
                std::cout << std::endl;
            }

            // This is a physical DC offset added to the signal
            // while ADQ_SetGainAndOffset is a digital calculation
            CHECKZERO(ADQ_SetAdjustableBias(adq_cu_ptr, adq_num, channel + 1, DC_offset));
        }
    }

    // FIXME: This wait time, after the bias is adjusted, seems important for
    //        the new settings to take place. It might me, though, that the ABCD
    //        internal delays are enough for it.
    //if (ADQ_HasAdjustableBias(adq_cu_ptr, adq_num) > 0) {
    //    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    //}

    // -------------------------------------------------------------------------
    //  DBS settings
    // -------------------------------------------------------------------------

    for (unsigned int instance = 0; instance < GetDBSInstancesNumber(); instance++) {
        const bool DBS_disabled = DBS_disableds[instance];
        // We are assuming that the channels correspond to the DBS instances
        const int DC_offset = DC_offsets[instance];

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Configure() ";
            std::cout << "Setting DBS instance: " << instance << " to: ";
            std::cout << (DBS_disabled ? "disabled" : "enabled") << "; ";
            std::cout << std::endl;
        }

        // TODO: Enable these features
        CHECKZERO(ADQ_SetupDBS(adq_cu_ptr, adq_num, instance,
                                                      DBS_disabled,
                                                      DC_offset,
                                                      default_DBS_saturation_level_lower,
                                                      default_DBS_saturation_level_upper));
    }

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Configure() ";
        std::cout << "Channels acquisition mask: " << (unsigned int)channels_acquisition_mask << "; ";
        std::cout << "Channels triggering mask: " << channels_triggering_mask << "; ";
        std::cout << std::endl;
    }

    // Only a set of possible masks are allowed, if the mask is wrong we use
    // the most conservative option of enabling all channels.
    if (!(channels_triggering_mask == 0 || // No channel, probably external trigger
          channels_triggering_mask == 1 || // Channel 0
          channels_triggering_mask == 2 || // Channel 1
          channels_triggering_mask == 4 || // Channel 2
          channels_triggering_mask == 8 || // Channel 3
          channels_triggering_mask == 3 || // Channel 0 and 1
          channels_triggering_mask == 12 ||// Channel 2 and 3
          channels_triggering_mask == 15)) // All channels
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Configure() ";
        std::cout << WRITE_YELLOW << "WARNING" << WRITE_NC << ": Wrong triggering mask (got: " << channels_triggering_mask << "), enabling all channels; ";
        std::cout << std::endl;

        channels_triggering_mask = 15;
    }

    // -------------------------------------------------------------------------
    //  Trigger settings
    // -------------------------------------------------------------------------

    // TODO: Enable this feature
    CHECKZERO(ADQ_DisarmTriggerBlocking(adq_cu_ptr, adq_num));
    //CHECKZERO(ADQ_SetupTriggerBlocking(adq_cu_ptr, adq_num, ADQ_TRIG_BLOCKING_DISABLE, 0, 0, 0));

    // TODO: Enable this feature
    CHECKZERO(ADQ_DisarmTimestampSync(adq_cu_ptr, adq_num));
    //CHECKZERO(ADQ_SetupTimestampSync(adq_cu_ptr, adq_num, ADQ_SYNCHRONIZATION_MODE_DISABLE, trigger_mode));

    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Configure() ";
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
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Configure() ";
            std::cout << "Setting external TTL trigger; ";
            std::cout << std::endl;
        }

        CHECKZERO(ADQ_SetExternTrigEdge(adq_cu_ptr, adq_num, trigger_slope));
        CHECKZERO(ADQ_SetExtTrigThreshold(adq_cu_ptr, adq_num, 1, trigger_level));
        //CHECKZERO(ADQ_SetExternalTriggerDelay(adq_cu_ptr, adq_num,  trig_external_delay));
    } else if (trigger_mode == ADQ_LEVEL_TRIGGER_MODE) {
        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Configure() ";
            std::cout << "Setting channels trigger; ";
            std::cout << std::endl;
        }

        CHECKZERO(ADQ_SetTriggerMode(adq_cu_ptr,  adq_num, trigger_mode));
        CHECKZERO(ADQ_SetLvlTrigLevel(adq_cu_ptr, adq_num, trigger_level));
        CHECKZERO(ADQ_SetLvlTrigEdge(adq_cu_ptr,  adq_num, trigger_slope));
        CHECKZERO(ADQ_SetLvlTrigChannel(adq_cu_ptr, adq_num, channels_triggering_mask));
    }

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Configure() ";
        std::cout << "Trigger from device: ";
        std::cout << ADQ_descriptions::trigger_mode.at(ADQ_GetTriggerMode(adq_cu_ptr, adq_num)) << "; ";
        std::cout << "Channels triggering mask: " << ADQ_GetLvlTrigChannel(adq_cu_ptr, adq_num) << "; ";
        std::cout << "Level: " << ADQ_GetLvlTrigLevel(adq_cu_ptr, adq_num) << "; ";
        std::cout << "Edge: " << ADQ_descriptions::trigger_slope.at(ADQ_GetLvlTrigEdge(adq_cu_ptr, adq_num)) << "; ";
        std::cout << std::endl;
    }

    // -------------------------------------------------------------------------
    //  Input impedances
    // -------------------------------------------------------------------------

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Configure() ";
        std::cout << "Setting ports input impedances; ";
        std::cout << "trig port: " << ADQ_descriptions::input_impedance.at(trig_port_input_impedance) << "; ";
        std::cout << "sync port: " << ADQ_descriptions::input_impedance.at(sync_port_input_impedance) << "; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_SetTriggerInputImpedance(adq_cu_ptr, adq_num, 1, trig_port_input_impedance));
    CHECKZERO(ADQ_SetTriggerInputImpedance(adq_cu_ptr, adq_num, 2, sync_port_input_impedance));

    // -------------------------------------------------------------------------
    //  Waveforms settings
    // -------------------------------------------------------------------------

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Configure() ";
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
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::SetChannelsNumber() ";
        std::cout << "Setting channels number to: " << n << "; ";
        std::cout << std::endl;
    }

    Digitizer::SetChannelsNumber(n);

    desired_input_ranges.resize(n, 1000);
    DC_offsets.resize(n, default_DC_offset);
    DBS_disableds.resize(n, false);
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::StartAcquisition()
{
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::StartAcquisition() ";
        std::cout << "Starting acquisition; ";
        std::cout << "Trigger mode: " << ADQ_descriptions::trigger_mode.at(trigger_mode) << "; ";
        std::cout << std::endl;
    }

    RearmTrigger();

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::RearmTrigger()
{
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::RearmTrigger() ";
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

bool ABCD::ADQ14_FWDAQ::AcquisitionReady()
{

    const unsigned int retval = ADQ_GetAcquiredAll(adq_cu_ptr, adq_num);

    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::AcquisitionReady() ";
        std::cout << "Acquisition ready: " << retval << "; ";
        std::cout << std::endl;
    }

    return retval == 1 ? true : false;
}

//==============================================================================

bool ABCD::ADQ14_FWDAQ::DataOverflow()
{

    const unsigned int retval = ADQ_GetStreamOverflow(adq_cu_ptr, adq_num);

    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::DataOverflow() ";
        std::cout << "Overflow: " << retval << "; ";
        std::cout << std::endl;
    }

    return retval == 0 ? false : true;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::GetWaveformsFromCard(std::vector<struct event_waveform> &waveforms)
{
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::GetWaveformsFromCard() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Error in fetching data; ";
        std::cout << std::endl;

        return DIGITIZER_FAILURE;
    } else if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::GetWaveformsFromCard() ";
        std::cout << "Collected all samples; ";
        std::cout << std::endl;
    }

    for (unsigned int record_index = 0; record_index < records_number; record_index++) {
        uint8_t record_status = target_headers[record_index * ADQ14_FWDAQ_RECORD_HEADER_SIZE];

        if ((record_status & ADQ14_FWDAQ_RECORD_HEADER_MASK_LOST_RECORD) > 0 ||
            (record_status & ADQ14_FWDAQ_RECORD_HEADER_MASK_LOST_DATA) > 0) {
            if (GetVerbosity() > 0) {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::GetWaveformsFromCard() ";
                std::cout << WRITE_YELLOW << "WARNING" << WRITE_NC << ": Lost data in record: " << record_index << "; ";
                std::cout << std::endl;
            }
        } else {
            const uint8_t ADQ_channel = target_headers[record_index * ADQ14_FWDAQ_RECORD_HEADER_SIZE + 2];
            const uint32_t ADQ_record_number = *((uint32_t*)(target_headers.data() + (record_index * ADQ14_FWDAQ_RECORD_HEADER_SIZE + 8)));

            if (GetVerbosity() > 2)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::GetWaveformsFromCard() ";
                std::cout << "Record number as read from ADQ: " << (unsigned int)ADQ_record_number << "; ";
                std::cout << std::endl;
            }

            // If we see a jump backward in timestamp bigger than half the timestamp
            // range we assume that there was an overflow in the timestamp counter.
            // A smaller jump could mean that the records in the buffer are not
            // sorted according to the timestamp, that would make sense but we have
            // not verified it.
            const int64_t timestamp_negative_difference = timestamp_last - target_timestamps[record_index];

            if (timestamp_negative_difference > ADQ14_FWDAQ_TIMESTAMP_THRESHOLD) {
                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::GetWaveformsFromCard() ";
                    std::cout << WRITE_YELLOW << "WARNING" << WRITE_NC << ": Detected timestamp overflow; ";
                    std::cout << "Overflows: " << timestamp_overflows << "; ";
                    std::cout << "Negative difference: " << (long long)timestamp_negative_difference << "; ";
                    std::cout << std::endl;
                }

                timestamp_offset += ADQ14_FWDAQ_TIMESTAMP_MAX;
                timestamp_overflows += 1;
            }

            // We do not add the offset here because we want to check the digitizer
            // behaviour and not the correction.
            timestamp_last = target_timestamps[record_index];

            const uint64_t timestamp_waveform = (target_timestamps[record_index] + timestamp_offset) << timestamp_bit_shift;

            for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++) {
                if (GetVerbosity() > 2)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::GetWaveformsFromCard() ";
                    std::cout << "Channel: " << channel << "; ";
                    std::cout << "as read from ADQ: " << (unsigned int)ADQ_channel << "; ";
                    std::cout << std::endl;
                }

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
    }

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::GetWaveformsFromCard() ";
        std::cout << "Converted all samples; ";
        std::cout << "Timestamp overflows: " << timestamp_overflows << "; ";
        std::cout << std::endl;
    }

    //ADQ_GetOverflow(adq_cu_ptr, adq_num);

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::StopAcquisition()
{
    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::ForceSoftwareTrigger()
{
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ForceSoftwareTrigger() ";
        std::cout << "Forcing a software trigger; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_DisarmTrigger(adq_cu_ptr, adq_num));
    CHECKZERO(ADQ_ArmTrigger(adq_cu_ptr, adq_num));
    CHECKZERO(ADQ_SWTrig(adq_cu_ptr, adq_num));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::ResetOverflow()
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ResetOverflow() ";
        std::cout << "Resetting a data overflow; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_OVERFLOW));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::ReadConfig(json_t *config)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Wrong clock source";
        std::cout << std::endl;
    }

    json_object_set_nocheck(config, "clock_source", json_string(ADQ_descriptions::clock_source.at(clock_source).c_str()));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
        std::cout << "Clock source: got: " << ADQ_descriptions::clock_source.at(clock_source) << " (index: " << clock_source << "); ";
        std::cout << std::endl;
    }

    timestamp_bit_shift = json_number_value(json_object_get(config, "timestamp_bit_shift"));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
        std::cout << "Timestamp bit shift: " << timestamp_bit_shift << " bits; ";
        std::cout << std::endl;
    }

    json_object_set_nocheck(config, "timestamp_bit_shift", json_integer(timestamp_bit_shift));

    // -------------------------------------------------------------------------
    //  Reading the trigger configuration
    // -------------------------------------------------------------------------
    json_t *trigger_config = json_object_get(config, "trigger");

    if (!json_is_object(trigger_config)) {
        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Configure() ";
            std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Missing \"trigger\" entry in configuration; ";
            std::cout << std::endl;
        }

        trigger_config = json_object();
        json_object_set_new_nocheck(config, "trigger", trigger_config);
    }

    const char *cstr_trigger_source = json_string_value(json_object_get(trigger_config, "source"));
    const std::string str_trigger_source = (cstr_trigger_source) ? std::string(cstr_trigger_source) : std::string();

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
        std::cout << "Trigger slope: " << str_trigger_slope<< "; ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Invalid trigger slope; ";
        std::cout << "Got: " << str_trigger_slope << "; ";
        std::cout << std::endl;
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

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
        std::cout << "Trigger level: " << trigger_level << "; ";
        std::cout << std::endl;
    }

    const char *cstr_trigger_impedance = json_string_value(json_object_get(trigger_config, "impedance"));
    const std::string str_trigger_impedance = (cstr_trigger_impedance) ? std::string(cstr_trigger_impedance) : std::string();

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
        std::cout << "Trig port impedance: " << str_trigger_impedance<< "; ";
        std::cout << std::endl;
    }

    // Looking for the settings in the description map
    const auto tii_result = map_utilities::find_item(ADQ_descriptions::input_impedance, str_trigger_impedance);

    if (tii_result != ADQ_descriptions::input_impedance.end() && str_trigger_impedance.length() > 0) {
        trig_port_input_impedance = tii_result->first;
    } else {
        trig_port_input_impedance = ADQ_IMPEDANCE_HIGH;

        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Invalid trigger impedance; ";
        std::cout << "Got: " << str_trigger_impedance << "; ";
        std::cout << std::endl;
    }

    json_object_set_nocheck(trigger_config, "impedance", json_string(ADQ_descriptions::input_impedance.at(trig_port_input_impedance).c_str()));

    // -------------------------------------------------------------------------
    //  Starting the sync configuration
    // -------------------------------------------------------------------------
    json_t *sync_config = json_object_get(config, "sync");

    if (!json_is_object(sync_config)) {
        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::Configure() ";
            std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Missing \"sync\" entry in configuration; ";
            std::cout << std::endl;
        }

        sync_config = json_object();
        json_object_set_new_nocheck(config, "sync", sync_config);
    }

    const char *cstr_sync_impedance = json_string_value(json_object_get(sync_config, "impedance"));
    const std::string str_sync_impedance = (cstr_sync_impedance) ? std::string(cstr_sync_impedance) : std::string();

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
        std::cout << "Sync port impedance: " << str_sync_impedance<< "; ";
        std::cout << std::endl;
    }

    // Looking for the settings in the description map
    const auto tsii_result = map_utilities::find_item(ADQ_descriptions::input_impedance, str_sync_impedance);

    if (tsii_result != ADQ_descriptions::input_impedance.end() && str_sync_impedance.length() > 0) {
        sync_port_input_impedance = tsii_result->first;
    } else {
        sync_port_input_impedance = ADQ_IMPEDANCE_HIGH;

        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Invalid sync impedance; ";
        std::cout << "Got: " << str_sync_impedance << "; ";
        std::cout << std::endl;
    }

    json_object_set_nocheck(sync_config, "impedance", json_string(ADQ_descriptions::input_impedance.at(sync_port_input_impedance).c_str()));

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
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
                    std::cout << "Found channel: " << id << "; ";
                    std::cout << std::endl;
                }

                const bool enabled = json_is_true(json_object_get(value, "enable"));

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
                    std::cout << "Channel is " << (enabled ? "enabled" : "disabled") << "; ";
                    std::cout << std::endl;
                }

                json_object_set_new_nocheck(value, "enable", json_boolean(enabled));

                const bool triggering = json_is_true(json_object_get(value, "triggering"));

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
                    std::cout << "Channel is " << (triggering ? "" : "not ") << "triggering; ";
                    std::cout << std::endl;
                }

                json_object_set_new_nocheck(value, "triggering", json_boolean(triggering));

                float input_range = json_number_value(json_object_get(value, "input_range"));

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
                    std::cout << "Input range: " << input_range << " mVpp; ";
                    std::cout << std::endl;
                }

                if (input_range <= 0) {
                    input_range = default_input_range;
                }

                json_object_set_new_nocheck(value, "input_range", json_real(input_range));

                const int offset = json_number_value(json_object_get(value, "DC_offset"));

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
                    std::cout << "DC offset: " << offset << " samples; ";
                    std::cout << std::endl;
                }

                json_object_set_new_nocheck(value, "DC_offset", json_integer(offset));

                const bool DBS_disable = json_is_true(json_object_get(value, "DBS_disable"));

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
                    std::cout << "DBS disable: " << (DBS_disable ? "true" : "false") << "; ";
                    std::cout << std::endl;
                }

                if (0 <= id && id < static_cast<int>(GetChannelsNumber())) {
                    SetChannelEnabled(id, enabled);
                    SetChannelTriggering(id, triggering);
                    desired_input_ranges[id] = input_range;
                    DC_offsets[id] = offset;
                    DBS_disableds[id] = DBS_disable;
                } else {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
        std::cout << "Pretrigger: " << pretrigger << "; ";
        std::cout << std::endl;
    }

    json_object_set_nocheck(config, "pretrigger", json_integer(pretrigger));

    const uint32_t scope_samples = json_integer_value(json_object_get(config, "scope_samples"));

    if (scope_samples < 1) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Samples number out of range";
        std::cout << std::endl;

        if (scope_samples < 1) {
            samples_per_record = 1;
        }
    } else {
        samples_per_record = scope_samples;
    }

    json_object_set_nocheck(config, "scope_samples", json_integer(samples_per_record));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
        std::cout << "Maximum records number: " << max_records_number << " (for scope_samples: " << samples_per_record << "); ";
        std::cout << std::endl;
    }

    records_number = json_integer_value(json_object_get(config, "records_number"));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
        std::cout << "Number of records: got: " << records_number << "; ";
        std::cout << std::endl;
    }

    if (records_number < 1 || max_records_number < records_number) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::ReadConfig() ";
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

int ABCD::ADQ14_FWDAQ::SpecificCommand(json_t *json_command)
{
    const char *cstr_command = json_string_value(json_object_get(json_command, "command"));
    const std::string command = (cstr_command) ? std::string(cstr_command) : std::string();

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::SpecificCommand() ";
        std::cout << "Specific command: " << command << "; ";
        std::cout << std::endl;
    }

    if (command == std::string("GPIO_set_direction")) {
        const int port = json_number_value(json_object_get(json_command, "port"));
        const int direction = json_number_value(json_object_get(json_command, "direction"));
        const int mask = json_number_value(json_object_get(json_command, "mask"));

        const int result = GPIOSetDirection(port, direction, mask);

        return result;

    } else if (command == std::string("GPIO_pulse")) {
        const int port = json_integer_value(json_object_get(json_command, "port"));
        const int width = json_integer_value(json_object_get(json_command, "width"));
        const int mask = json_number_value(json_object_get(json_command, "mask"));

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::SpecificCommand() ";
            std::cout << "Pulse on GPIO of width: " << width << " us; ";
            std::cout << std::endl;
        }

        const int result = GPIOPulse(port, width, mask);

        return result;

    } else if (command == std::string("timestamp_reset")) {
        // ---------------------------------------------------------------------
        //  Timestamp reset
        // ---------------------------------------------------------------------

        const bool timestamp_reset_arm = json_is_true(json_object_get(json_command, "arm"));

        if (timestamp_reset_arm) {
            // -----------------------------------------------------------------
            //  Arming the timestamp reset
            // -----------------------------------------------------------------
            const char *cstr_timestamp_reset_mode = json_string_value(json_object_get(json_command, "mode"));
            const std::string str_timestamp_reset_mode = (cstr_timestamp_reset_mode) ? std::string(cstr_timestamp_reset_mode) : std::string();

            const char *cstr_timestamp_reset_source = json_string_value(json_object_get(json_command, "source"));
            const std::string str_timestamp_reset_source = (cstr_timestamp_reset_source) ? std::string(cstr_timestamp_reset_source) : std::string();

            const int result = TimestampResetArm(str_timestamp_reset_mode, str_timestamp_reset_source);

            return result;

        } else {
            // -----------------------------------------------------------------
            //  Disarming the timestamp reset
            // -----------------------------------------------------------------
            if (GetVerbosity() > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::SpecificCommand() ";
                std::cout << "Disarming timestamp reset; ";
                std::cout << std::endl;
            }

            const int result = TimestampResetDisarm();

            return result;
        }
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::GPIOSetDirection(int port, int direction, int mask)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::GPIOSetDirection() ";
        std::cout << "Setting GPIO direction: " << direction << "; ";
        std::cout << "mask: " << mask << "; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_EnableGPIOPort(adq_cu_ptr, adq_num, port, 1));
    CHECKZERO(ADQ_SetDirectionGPIOPort(adq_cu_ptr, adq_num, port, direction, mask));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::GPIOPulse(int port, int width, int mask)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::GPIOPulse() ";
        std::cout << "Pulse on GPIO of width: " << width << " us; ";
        std::cout << std::endl;
    }

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
    // -----------------------------------------------------------------
    //  Arming the timestamp reset
    // -----------------------------------------------------------------
    unsigned int timestamp_reset_mode = ADQ_SYNCHRONIZATION_MODE_FIRST;

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::TimestampResetArm() ";
        std::cout << "Timestamp reset mode: " << mode << "; ";
        std::cout << std::endl;
    }

    // Looking for the settings in the description map
    const auto tsm_result = map_utilities::find_item(ADQ_descriptions::ADQ14_timestamp_synchronization_mode, mode);

    if (tsm_result != ADQ_descriptions::ADQ14_timestamp_synchronization_mode.end() && mode.length() > 0) {
        timestamp_reset_mode = tsm_result->first;
    } else {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::TimestampResetArm() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Invalid timestamp_reset mode; ";
        std::cout << "Got: " << mode << "; ";
        std::cout << std::endl;

        return DIGITIZER_FAILURE;
    }

    unsigned int timestamp_reset_source = ADQ_EVENT_SOURCE_SYNC;

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::TimestampResetArm() ";
        std::cout << "Timestamp synchronization source: " << source<< "; ";
        std::cout << std::endl;
    }

    // Looking for the settings in the description map
    const auto tss_result = map_utilities::find_item(ADQ_descriptions::ADQ14_timestamp_synchronization_source, source);

    if (tss_result != ADQ_descriptions::ADQ14_timestamp_synchronization_source.end() && source.length() > 0) {
        timestamp_reset_source = tss_result->first;
    } else {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::TimestampResetArm() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Invalid timestamp_reset source; ";
        std::cout << "Got: " << source << "; ";
        std::cout << std::endl;

        return DIGITIZER_FAILURE;
    }

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::TimestampResetArm() ";
        std::cout << "Arming timestamp synchronization; ";
        std::cout << "mode: " << ADQ_descriptions::ADQ14_timestamp_synchronization_mode.at(timestamp_reset_mode) << "; ";
        std::cout << "source: " << ADQ_descriptions::ADQ14_timestamp_synchronization_source.at(timestamp_reset_source) << "; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_DisarmTimestampSync(adq_cu_ptr, adq_num));
    CHECKZERO(ADQ_SetupTimestampSync(adq_cu_ptr, adq_num, timestamp_reset_mode, timestamp_reset_source));
    CHECKZERO(ADQ_ArmTimestampSync(adq_cu_ptr, adq_num));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWDAQ::TimestampResetDisarm()
{
    // -----------------------------------------------------------------
    //  Disarming the timestamp reset
    // -----------------------------------------------------------------
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWDAQ::TimestampResetDisarm() ";
        std::cout << "Disarming timestamp reset; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_DisarmTimestampSync(adq_cu_ptr, adq_num));

    return DIGITIZER_SUCCESS;
}

//==============================================================================
