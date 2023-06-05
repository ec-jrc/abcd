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
#include "ADQ14_FWPD.hpp"

#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define BUFFER_SIZE 32

// Defined in V
const double ABCD::ADQ14_FWPD::default_trig_ext_threshold = 0.5;
const unsigned int ABCD::ADQ14_FWPD::default_trig_ext_slope = ADQ_TRIG_SLOPE_RISING;
// Defined in mVpp
const float ABCD::ADQ14_FWPD::default_input_range = 1000;
// Defined in ADC samples
const int ABCD::ADQ14_FWPD::default_DC_offset = 31000;
// Defined in ADC samples
const int ABCD::ADQ14_FWPD::default_DBS_target = 31000;
// If left at zero the FWPD will use its default values
const int ABCD::ADQ14_FWPD::default_DBS_saturation_level_lower = 0;
const int ABCD::ADQ14_FWPD::default_DBS_saturation_level_upper = 0;

const unsigned int ABCD::ADQ14_FWPD::default_DMA_flush_timeout = 1000;

ABCD::ADQ14_FWPD::ADQ14_FWPD(int Verbosity) : Digitizer(Verbosity)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ADQ14_FWPD() ";
        std::cout << std::endl;
    }

    SetModel("ADQ14_FWPD");

    adq_cu_ptr = NULL;
    adq_num = 0;

    streaming_generation = 1;

    SetEnabled(false);

    clock_source = ADQ_CLOCK_INT_INTREF;

    DBS_instances_number = 0;

    trig_mode = ADQ_LEVEL_TRIGGER_MODE;
    trig_external_delay = 0;

    trig_port_input_impedance = ADQ_IMPEDANCE_HIGH;
    sync_port_input_impedance = ADQ_IMPEDANCE_HIGH;

    // This is reset only at the class creation because the timestamp seems to
    // be growing continuously even after starts or stops.
    timestamp_last = 0;
    timestamp_offset = 0;
    timestamp_overflows = 0;
}

//==============================================================================

ABCD::ADQ14_FWPD::~ADQ14_FWPD() {
    for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++) {
        if (target_buffers[channel]) {
            delete target_buffers[channel];
        }
        if (target_headers[channel]) {
            delete target_headers[channel];
        }
    }
}

//==============================================================================

int ABCD::ADQ14_FWPD::Initialize(void* adq, int num)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Initialize() ";
        std::cout << std::endl;
    }

    adq_cu_ptr = adq;
    adq_num = num;

    CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_POWER_ON));
    CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_COMMUNICATION));
    CHECKZERO(ADQ_ResetDevice(adq_cu_ptr, adq_num, RESET_ADC_DATA));

    const char *board_name = ADQ_GetBoardSerialNumber(adq_cu_ptr, adq_num);

    if (!board_name) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Error in getting the serial number; ";
        std::cout << std::endl;

        SetName("");
    } else {
        SetName(board_name);
    }

    const unsigned int number_of_channels = ADQ_GetNofChannels(adq_cu_ptr, adq_num);

    SetChannelsNumber(number_of_channels);

    CHECKZERO(ADQ_PDGetGeneration(adq_cu_ptr, adq_num, &FWPD_generation));

    // For the first generation of the FWPD we could not get the other streaming
    // approaches working... We force then the triggered streaming.
    if (FWPD_generation == 1) {
        streaming_generation = 1;
    } else {
        streaming_generation = 3;
    }

    unsigned int adc_cores = 0;
    CHECKZERO(ADQ_GetNofAdcCores(adq_cu_ptr, adq_num, &adc_cores));

    unsigned int dbs_inst = 0;
    CHECKZERO(ADQ_GetNofDBSInstances(adq_cu_ptr, adq_num, &dbs_inst));

    SetDBSInstancesNumber(dbs_inst);

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Initialize() ";
        std::cout << "Initialized board; ";
        std::cout << std::endl;
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Initialize() ";
        std::cout << "Card name (serial number): " << GetName() << "; ";
        std::cout << "Product name: " << ADQ_GetBoardProductName(adq_cu_ptr, adq_num) << "; ";
        std::cout << "Card option: " << ADQ_GetCardOption(adq_cu_ptr, adq_num) << "; ";
        std::cout << std::endl;

        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Initialize() ";
        std::cout << "USB address: " << ADQ_GetUSBAddress(adq_cu_ptr, adq_num) << "; ";
        std::cout << "PCIe address: " << ADQ_GetPCIeAddress(adq_cu_ptr, adq_num) << "; ";
        std::cout << std::endl;

        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Initialize() ";
        std::cout << "ADQAPI Revision: " << ADQAPI_GetRevision() << "; ";
        std::cout << "PD Firmware generation: " << FWPD_generation << "; ";
        std::cout << "Streaming generation: " << streaming_generation << "; ";
        std::cout << "ADQ14 Revision: {";
        int* revision = ADQ_GetRevision(adq_cu_ptr, adq_num);
        for (int i = 0; i < 6; i++) {
            std::cout << revision[i] << ", ";
        }
        std::cout << "}; ";
        std::cout << std::endl;

        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Initialize() ";
        std::cout << "Channels number: " << GetChannelsNumber() << "; ";
        std::cout << "ADC cores: " << adc_cores << "; ";
        std::cout << "DBS instances: " << GetDBSInstancesNumber() << "; ";
        std::cout << std::endl;

        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Initialize() ";
        std::cout << "Has adjustable input range: " << (ADQ_HasAdjustableInputRange(adq_cu_ptr, adq_num) > 0 ? "true" : "false") << "; ";
        std::cout << "Has adjustable offset: " << (ADQ_HasAdjustableBias(adq_cu_ptr, adq_num) > 0 ? "true" : "false") << "; ";
        // FIXME: Find out what trigger_num is to be used
        //std::cout << "Has adjustable external trigger threshold: " << (ADQ_HasVariableTrigThreshold(adq_cu_ptr, adq_num, trigger_num) > 0 ? "true" : "false") << "; ";
        std::cout << std::endl;

        for (auto &pair : ADQ_descriptions::ADQ14_temperatures) {
            const double temperature = ADQ_GetTemperature(adq_cu_ptr, adq_num, pair.first) / 256.0;

            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Initialize() ";
            std::cout << pair.second << " temperature: " << temperature << "; ";
            std::cout << std::endl;
        }
    }

    CHECKZERO(ADQ_SetOvervoltageProtection(adq_cu_ptr, adq_num, ADQ_OVERVOLTAGE_PROTECTION_ENABLE));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWPD::Configure()
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
        std::cout << "Setting clock; ";
        std::cout << "clock_source: " << ADQ_descriptions::clock_source.at(clock_source) << "; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_SetClockSource(adq_cu_ptr, adq_num, clock_source));

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
        std::cout << "Clock source from device: " << ADQ_GetClockSource(adq_cu_ptr, adq_num) << "; ";
        try {
            std::cout << ADQ_descriptions::clock_source.at(ADQ_GetClockSource(adq_cu_ptr, adq_num)) << "; ";
        } catch (...) {
        }
        std::cout << std::endl;
    }

    // -------------------------------------------------------------------------
    //  Channels settings
    // -------------------------------------------------------------------------

    for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++) {
        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
            std::cout << "Channel: " << channel << "; ";
            std::cout << "Enabled: " << (IsChannelEnabled(channel) ? "true" : "false") << "; ";
            std::cout << "Triggering: " << (IsChannelTriggering(channel) ? "true" : "false") << "; ";
            std::cout << std::endl;
        }

        if (ADQ_HasAdjustableInputRange(adq_cu_ptr, adq_num) > 0) {
            if (GetVerbosity() > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
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
                std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
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
                std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
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
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
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

    // -------------------------------------------------------------------------
    //  Trigger settings
    // -------------------------------------------------------------------------

    // FIXME: This feature is giving an error
    //CHECKZERO(ADQ_DisarmTriggerBlocking(adq_cu_ptr, adq_num));
    //CHECKZERO(ADQ_SetupTriggerBlocking(adq_cu_ptr, adq_num, ADQ_TRIG_BLOCKING_DISABLE, 0, 0, 0));

    // FIXME: Check whether this is in conflict with PDSetupLevelTrig
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
        std::cout << "Setting trigger; ";
        std::cout << "mode: " << ADQ_descriptions::trig_mode.at(trig_mode) << "; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_SetTriggerMode(adq_cu_ptr, adq_num, trig_mode));

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
        std::cout << "Trigger from device: ";
        std::cout << ADQ_descriptions::trig_mode.at(ADQ_GetTriggerMode(adq_cu_ptr, adq_num)) << "; ";
        std::cout << std::endl;
    }

    if (trig_mode == ADQ_EXT_TRIGGER_MODE) {
        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
            std::cout << "Setting external TTL trigger; ";
            std::cout << std::endl;
        }

        // TODO: Enable these features
        CHECKZERO(ADQ_SetExtTrigThreshold(adq_cu_ptr, adq_num, 1, default_trig_ext_threshold));
        CHECKZERO(ADQ_SetExternTrigEdge(adq_cu_ptr, adq_num, default_trig_ext_slope));
        //CHECKZERO(ADQ_SetExternalTriggerDelay(adq_cu_ptr, adq_num,  trig_external_delay));
    }

    // -------------------------------------------------------------------------
    //  Input impedances
    // -------------------------------------------------------------------------

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
        std::cout << "Setting ports input impedances; ";
        std::cout << "trig port: " << ADQ_descriptions::input_impedance.at(trig_port_input_impedance) << "; ";
        std::cout << "sync port: " << ADQ_descriptions::input_impedance.at(sync_port_input_impedance) << "; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_SetTriggerInputImpedance(adq_cu_ptr, adq_num, 1, trig_port_input_impedance));
    CHECKZERO(ADQ_SetTriggerInputImpedance(adq_cu_ptr, adq_num, 2, sync_port_input_impedance));

    // -------------------------------------------------------------------------
    //  FWPD specific settings
    // -------------------------------------------------------------------------

    channels_acquisition_mask = 0;
    uint32_t max_scope_samples = 0;
    int32_t max_pretrigger = 0;
    int32_t max_records_number = 0;

    // These are always set in any case in the example, even with an external trigger
    for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++) {
        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
            std::cout << "FWPD Settings of channel: " << channel << "; ";
            std::cout << "Enabled: " << (IsChannelEnabled(channel) ? "true" : "false") << "; ";
            std::cout << std::endl;
        }

        if (IsChannelEnabled(channel)) {
            channels_acquisition_mask += (1 << channel);

            if (max_pretrigger < pretriggers[channel]) {
                max_pretrigger = pretriggers[channel];
            }
            if (max_scope_samples < scope_samples[channel]) {
                max_scope_samples = scope_samples[channel];
            }
            if (max_records_number < records_numbers[channel]) {
                max_records_number = records_numbers[channel];
            }

            // TODO: Enable these features
            const int reset_hysteresis = trig_hysteresises[channel];
            const int trig_arm_hysteresis = trig_hysteresises[channel];
            const int reset_arm_hysteresis = trig_hysteresises[channel];
            const int reset_polarity = (trig_slopes[channel] == ADQ_TRIG_SLOPE_RISING ? ADQ_TRIG_SLOPE_FALLING : ADQ_TRIG_SLOPE_RISING);

            CHECKZERO(ADQ_PDSetupLevelTrig(adq_cu_ptr, adq_num, channel + 1,
                                           trig_levels[channel],
                                           reset_hysteresis,
                                           trig_arm_hysteresis,
                                           reset_arm_hysteresis,
                                           trig_slopes[channel],
                                           reset_polarity));


            // TODO: Enable these features
            const unsigned int record_variable_length = false ? 1 : 0;

            CHECKZERO(ADQ_PDSetupTiming(adq_cu_ptr, adq_num, channel + 1,
                                        pretriggers[channel],
                                        smooth_samples[channel],
                                        smooth_delays[channel],
                                        scope_samples[channel],
                                        records_numbers[channel],
                                        record_variable_length));

            // TODO: Enable these features
            // Since this gives an error with the generation 1, I am assuming that
            // the problem is the firmware generation...
            //if (FWPD_generation >= 2) {
            //    const unsigned int collection_mode = ADQ_COLLECTION_MODE_RAW;
            //    const unsigned int reduction_factor = 1;
            //    const unsigned int detection_window_length = scope_samples[channel];
            //    const unsigned int padding_offset = scope_samples[channel];
            //    const unsigned int minimum_frame_length = 0;

            //    CHECKZERO(ADQ_PDSetupCharacterization(adq_cu_ptr, adq_num, channel + 1,
            //                                          collection_mode,
            //                                          reduction_factor,
            //                                          detection_window_length,
            //                                          scope_samples[channel],
            //                                          padding_offset,
            //                                          minimum_frame_length,
            //                                          trig_slopes[channel],
            //                                          trig_mode, trig_mode));
            //}
        }
    }

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
        std::cout << "Enabling PD level trigger; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_PDEnableTriggerCoincidence(adq_cu_ptr, adq_num, false ? 1 : 0));
    CHECKZERO(ADQ_PDEnableLevelTrig(adq_cu_ptr, adq_num, true ? 1 : 0));

    if (GetVerbosity() > 0)
    {
        unsigned int level_trigger_status = 0;

        CHECKZERO(ADQ_PDGetLevelTrigStatus(adq_cu_ptr, adq_num, &level_trigger_status));

        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
        std::cout << "Level trigger status: " << level_trigger_status << "; ";
        std::cout << std::endl;
    }

    // -------------------------------------------------------------------------
    //  Data mux
    // -------------------------------------------------------------------------

    for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++) {
        // Since this gives an error with the generation 1, I am assuming that
        // the problem is the firmware generation...
        if (FWPD_generation >= 2) {
            if (GetVerbosity() > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
                std::cout << "Setting channel multiplexer to default values; ";
                std::cout << std::endl;
            }

            CHECKZERO(ADQ_PDSetDataMux(adq_cu_ptr, adq_num, channel + 1, channel + 1));
        }

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
            std::cout << "Disabling test data; ";
            std::cout << std::endl;
        }

        // Disable test data and forward normal data to channels
        // in the example it is repeated at each channel, but to me it seems
        // unnecessary.
        CHECKZERO(ADQ_SetTestPatternMode(adq_cu_ptr, adq_num, 0));
    }

    // -------------------------------------------------------------------------
    //  Transfer settings
    // -------------------------------------------------------------------------

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
        std::cout << "Setting transfer buffers number to: " << transfer_buffers_number << "; ";
        std::cout << "Setting transfer buffer size to: " << (transfer_buffer_size / 1024.0) << " kiB; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_SetTransferBuffers(adq_cu_ptr, adq_num, transfer_buffers_number, transfer_buffer_size));

    // -------------------------------------------------------------------------
    //  Streaming setup
    // -------------------------------------------------------------------------

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
        std::cout << "Setting up PD streaming with channel mask: 0x" << std::hex << (unsigned int)channels_acquisition_mask << std::dec << "; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_PDSetupStreaming(adq_cu_ptr, adq_num, channels_acquisition_mask));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
        std::cout << "Using streaming generation: " << streaming_generation << "; ";
        std::cout << std::endl;
    }

    if (streaming_generation == 1) {
        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
            std::cout << "Allocating streaming memory; ";
            std::cout << std::endl;
        }

        for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++) {
            if (target_buffers[channel]) {
                delete target_buffers[channel];
            }
            if (target_headers[channel]) {
                delete target_headers[channel];
            }

            // FIXME: Check pointers
            target_buffers[channel] = new int16_t[transfer_buffer_size];
            // I do not know why is it 28...
            target_headers[channel] = new StreamingHeader_t[transfer_buffer_size / 28 * sizeof(StreamingHeader_t)];
        }

        //// Sets the flag enabling infinite records
        //const unsigned int number_of_records = 1 << 31;

        //// With this set-up we have to acquire the same number of samples for
        //// all channels thus we use the maxima

        //if (GetVerbosity() > 0)
        //{
        //    char time_buffer[BUFFER_SIZE];
        //    time_string(time_buffer, BUFFER_SIZE, NULL);
        //    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
        //    std::cout << "Triggered streaming setup; ";
        //    std::cout << "number_of_records: 0x" << std::hex << max_records_number << std::dec << ", " << max_records_number << "; ";
        //    std::cout << "scope_samples: " << (unsigned int)max_scope_samples << "; ";
        //    std::cout << "pretrigger: " << (int)max_pretrigger << "; ";
        //    std::cout << "channels_acquisition_mask: 0x" << std::hex << (unsigned int)channels_acquisition_mask << std::dec << "; ";
        //    std::cout << std::endl;
        //}
        //// This gives an error when used with the FWPD generation 1
        //CHECKZERO(ADQ_TriggeredStreamingSetup(adq_cu_ptr, adq_num,
        //                                      max_records_number,
        //                                      max_scope_samples,
        //                                      max_pretrigger,
        //                                      0,
        //                                      channels_acquisition_mask));

        //// This also gives an error when used with the FWPD generation 1
        //CHECKZERO(ADQ_SetStreamStatus(adq_cu_ptr, adq_num, 1));
        ////CHECKZERO(ADQ_SetStreamConfig(adq_cu_ptr, adq_num, 1, 0));
        ////CHECKZERO(ADQ_SetStreamConfig(adq_cu_ptr, adq_num, 2, 0));
        ////CHECKZERO(ADQ_SetStreamConfig(adq_cu_ptr, adq_num, 3, channels_acquisition_mask));
        ////CHECKZERO(ADQ_SetTriggerMode(adq_cu_ptr, adq_num, trig_mode));
    } else {
        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
            std::cout << "Initializing data readout parameters; ";
            std::cout << std::endl;
        }

        const size_t rpi = ADQ_InitializeParameters(adq_cu_ptr, adq_num, ADQ_PARAMETER_ID_DATA_READOUT, &readout_parameters);

        if (rpi != sizeof(readout_parameters)) {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
            std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Unable to initialize streaming parameters";
            std::cout << std::endl;

            return DIGITIZER_FAILURE;
        }

        // Momery is managed by the API, the memory consumption is bound for each channel
        readout_parameters.common.memory_owner = ADQ_MEMORY_OWNER_API;

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
            std::cout << "Setting data readout parameters; ";
            std::cout << std::endl;
        }

        const size_t rps = ADQ_SetParameters(adq_cu_ptr, adq_num, &readout_parameters);

        if (rps != sizeof(readout_parameters)) {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
            std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Unable to initialize streaming parameters";
            std::cout << std::endl;

            return DIGITIZER_FAILURE;
        }
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

void ABCD::ADQ14_FWPD::SetChannelsNumber(unsigned int n)
{
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::SetChannelsNumber() ";
        std::cout << "Setting channels number to: " << n << "; ";
        std::cout << std::endl;
    }

    Digitizer::SetChannelsNumber(n);

    desired_input_ranges.resize(n, 1000);
    trig_levels.resize(n, 0);
    trig_hysteresises.resize(n, 0);
    trig_slopes.resize(n, ADQ_TRIG_SLOPE_RISING);
    pretriggers.resize(n, 0);
    DC_offsets.resize(n, default_DC_offset);
    DBS_disableds.resize(n, false);
    smooth_samples.resize(n, ADQ_ADQ14_MAX_SMOOTH_SAMPLES);
    smooth_delays.resize(n, 0);
    scope_samples.resize(n, 1024);
    records_numbers.resize(n, ADQ_INFINITE_NOF_RECORDS);
    target_buffers.resize(n, nullptr);
    target_headers.resize(n, nullptr);
    added_samples.resize(n, 0);
    added_headers.resize(n, 0);
    status_headers.resize(n, 0);
    incomplete_records.resize(n, std::vector<int16_t>());
    remaining_samples.resize(n, 0);

}

//==============================================================================

int ABCD::ADQ14_FWPD::StartAcquisition()
{
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::StartAcquisition() ";
        std::cout << "Starting acquisition; ";
        std::cout << "Trigger mode: " << ADQ_descriptions::trig_mode.at(trig_mode) << "; ";
        std::cout << std::endl;
    }

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::StartAcquisition() ";
        std::cout << "Using streaming generation: " << streaming_generation << "; ";
        std::cout << std::endl;
    }

    last_buffer_ready = std::chrono::system_clock::now();

    if (streaming_generation == 1) {
        CHECKZERO(ADQ_TriggeredStreamingArm(adq_cu_ptr, adq_num));
        CHECKZERO(ADQ_StopStreaming(adq_cu_ptr, adq_num));
        CHECKZERO(ADQ_StartStreaming(adq_cu_ptr, adq_num));
    } else {
        const int retval = ADQ_StartDataAcquisition(adq_cu_ptr, adq_num);

        if (retval != ADQ_EOK) {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::StartAcquisition() ";
            std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Unable to start acquisition ";
            std::cout << " (code: " << WRITE_YELLOW << retval << WRITE_NC << "); ";
            std::cout << std::endl;
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::StartAcquisition() ";
            std::cout << WRITE_RED << ADQ_descriptions::error.at(retval) << WRITE_NC << "; ";
            std::cout << std::endl;

            return DIGITIZER_FAILURE;
        }
    }


    if (trig_mode == ADQ_SW_TRIGGER_MODE) {
        // We need to set it at least to 1 to force at least one trigger
        int max_records_number = 1;

        for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++) {
            if (records_numbers[channel] > max_records_number) {
                max_records_number = records_numbers[channel];
            }
        }

        for (int i = 0; i < max_records_number; i++) {
            ForceSoftwareTrigger();
        }
    }

    for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++) {
            remaining_samples[channel] = 0;

            incomplete_records[channel].resize(scope_samples[channel], 0);
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWPD::RearmTrigger()
{
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::RearmTrigger() ";
        std::cout << "Rearming trigger; ";
        std::cout << "Trigger mode: " << ADQ_descriptions::trig_mode.at(trig_mode) << " (index: " << trig_mode << "); ";
        std::cout << std::endl;
    }

    if (trig_mode == ADQ_SW_TRIGGER_MODE) {
        if (GetVerbosity() > 1)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::RearmTrigger() ";
            std::cout << "Forcing software trigger; ";
            std::cout << std::endl;
        }

        ForceSoftwareTrigger();
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

bool ABCD::ADQ14_FWPD::AcquisitionReady()
{
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::AcquisitionReady() ";
        std::cout << "Using streaming generation: " << streaming_generation << "; ";
        std::cout << std::endl;
    }

    if (streaming_generation == 1) {
        unsigned int filled_buffers = 0;

        CHECKZERO(ADQ_GetTransferBufferStatus(adq_cu_ptr, adq_num, &filled_buffers));

        const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
        auto delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_buffer_ready);

        if (GetVerbosity() > 1)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::AcquisitionReady() ";
            std::cout << "Filled buffers: " << filled_buffers << "; ";
            std::cout << "Time since the last buffer ready: " << delta_time.count() << " ms; ";
            std::cout << std::endl;
        }

        if (filled_buffers <= 0) {
            if (delta_time > std::chrono::milliseconds(DMA_flush_timeout))
            {
                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::AcquisitionReady() ";
                    std::cout << "Issuing a flush of the DMA; ";
                    std::cout << std::endl;
                }

                CHECKZERO(ADQ_FlushDMA(adq_cu_ptr, adq_num));
            }

            return false;
        } else {
            last_buffer_ready = std::chrono::system_clock::now();

            return true;
        }
    } else {
        // For this streaming generation there is no information whether data
        // is available or not, we can only try to read it
        return true;
    }
}

//==============================================================================

int ABCD::ADQ14_FWPD::GetWaveformsFromCard(std::vector<struct event_waveform> &waveforms)
{
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::GetWaveformsFromCard() ";
        std::cout << "Using streaming generation: " << streaming_generation << "; ";
        std::cout << std::endl;
    }

    if (streaming_generation == 1) {
        while (AcquisitionReady()) {
            // Putting a flush here makes the digitizer unstable and sometimes it gives an error.
            //if (GetVerbosity() > 0)
            //{
            //    char time_buffer[BUFFER_SIZE];
            //    time_string(time_buffer, BUFFER_SIZE, NULL);
            //    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::GetWaveformsFromCard() ";
            //    std::cout << "Issuing a flush of the DMA; ";
            //    std::cout << std::endl;
            //}
            //CHECKZERO(ADQ_FlushDMA(adq_cu_ptr, adq_num));

            // When this fails a subsequent flush of the DMA causes a segfault,
            // we should reset the board if it fails.
            //CHECKZERO(ADQ_GetDataStreaming(adq_cu_ptr, adq_num,
            //                               (void**)target_buffers.data(),
            //                               (void**)target_headers.data(),
            //                               channels_acquisition_mask,
            //                               added_samples.data(),
            //                               added_headers.data(),
            //                               status_headers.data()));
            const int retval = ADQ_GetDataStreaming(adq_cu_ptr, adq_num,
                                                    (void**)target_buffers.data(),
                                                    (void**)target_headers.data(),
                                                    channels_acquisition_mask,
                                                    added_samples.data(),
                                                    added_headers.data(),
                                                    status_headers.data());

            if (!retval) {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::GetWaveformsFromCard() ";
                std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": ADQ_GetDataStreaming failed; ";
                std::cout << std::endl;

                return DIGITIZER_FAILURE;
            }

            for (unsigned int channel = 0; channel < GetChannelsNumber(); channel++) {
                if (GetVerbosity() > 1)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::GetWaveformsFromCard() ";
                    std::cout << "Channel: " << channel << " (enabled: " << IsChannelEnabled(channel) << "); ";
                    std::cout << "Added samples: " << added_samples[channel] << "; ";
                    std::cout << "Added headers: " << added_headers[channel] << "; ";
                    std::cout << "Status headers: " << status_headers[channel] << "; ";
                    std::cout << std::endl;
                }

                if (IsChannelEnabled(channel) && added_headers[channel] > 0) {
                    if (GetVerbosity() > 1)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::GetWaveformsFromCard() ";
                        std::cout << "Available for channel " << channel << ": ";
                        std::cout << "headers: " << added_headers[channel] << "; ";
                        std::cout << "samples: " << added_samples[channel] << "; ";
                        std::cout << std::endl;
                    }

                    if (!status_headers[channel]) {
                        if (GetVerbosity() > 0)
                        {
                            char time_buffer[BUFFER_SIZE];
                            time_string(time_buffer, BUFFER_SIZE, NULL);
                            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::GetWaveformsFromCard() ";
                            std::cout << "Icomplete record at end; ";
                            std::cout << std::endl;
                        }

                        added_headers[channel] -= 1;
                    }

                    uint32_t samples_offset = 0;

                    for (unsigned int index = 0; index < added_headers[channel]; index++) {
                        const uint32_t record_number = target_headers[channel][index].RecordNumber;
                        const uint32_t samples_per_record = target_headers[channel][index].RecordLength;
                        const uint64_t timestamp = target_headers[channel][index].Timestamp;

                        if (GetVerbosity() > 1)
                        {
                            char time_buffer[BUFFER_SIZE];
                            time_string(time_buffer, BUFFER_SIZE, NULL);
                            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::GetWaveformsFromCard() ";
                            std::cout << "Channel: " << channel << "; ";
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

                        if (timestamp_negative_difference > ADQ14_FWPD_TIMESTAMP_THRESHOLD) {
                            if (GetVerbosity() > 0)
                            {
                                char time_buffer[BUFFER_SIZE];
                                time_string(time_buffer, BUFFER_SIZE, NULL);
                                std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::GetWaveformsFromCard() ";
                                std::cout << WRITE_YELLOW << "WARNING" << WRITE_NC << ": Detected timestamp overflow; ";
                                std::cout << "Overflows: " << timestamp_overflows << "; ";
                                std::cout << "Negative difference: " << (long long)timestamp_negative_difference << "; ";
                                std::cout << std::endl;
                            }

                            timestamp_offset += ADQ14_FWPD_TIMESTAMP_MAX;
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

                        // There is the possibility that there is an incomplete
                        // buffer left from the previous call of GetDataStreaming()
                        // If there was such an incomplete buffer then there are
                        // remaining_samples from before.
                        if (remaining_samples[channel] > 0) {
                            if (GetVerbosity() > 0)
                            {
                                char time_buffer[BUFFER_SIZE];
                                time_string(time_buffer, BUFFER_SIZE, NULL);
                                std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::GetWaveformsFromCard() ";
                                std::cout << "Collecting an incomplete record from previous call; ";
                                std::cout << "Remaining samples: " << remaining_samples[channel] << "; ";
                                std::cout << std::endl;
                            }

                            // Copy at the beginning of this new record the
                            // remaining samples from the previous call of
                            // GetDataStreaming()
                            for (uint32_t sample_index = 0; sample_index < remaining_samples[channel]; sample_index++)
                            {
                                const int16_t value = incomplete_records[channel][sample_index];

                                // We add an offset to match it to the rest of ABCD
                                // The 14 bit data is aligned to the MSB thus we should push it back
                                samples[sample_index] = ((value >> 2) + (1 << 15));
                            }
                            for (uint32_t sample_index = remaining_samples[channel]; sample_index < samples_per_record; sample_index++) {
                                const int16_t value = target_buffers[channel][samples_offset + sample_index - remaining_samples[channel]];

                                // We add an offset to match it to the rest of ABCD
                                // The 14 bit data is aligned to the MSB thus we should push it back
                                samples[sample_index] = ((value >> 2) + (1 << 15));
                            }

                            samples_offset += (samples_per_record - remaining_samples[channel]);

                            remaining_samples[channel] = 0;
                        } else {
                            for (uint32_t sample_index = 0; sample_index < samples_per_record; sample_index++) {
                                const int16_t value = target_buffers[channel][samples_offset + sample_index];

                                // We add an offset to match it to the rest of ABCD
                                // The 14 bit data is aligned to the MSB thus we should push it back
                                samples[sample_index] = ((value >> 2) + (1 << 15));
                            }

                            samples_offset += samples_per_record;
                        }

                        waveforms.push_back(this_waveform);
                    }

                    // The last record is incomplete and we should store it for
                    // the next call of GetDataStreaming
                    if (status_headers[channel] == 0) {
                        // Copying the incomplete record at the end
                        for (uint32_t sample_index = samples_offset; sample_index < added_samples[channel]; sample_index++)
                        {
                            incomplete_records[channel][sample_index - samples_offset] = target_buffers[channel][sample_index];
                        }

                        // This is the number of samples that are left from
                        // this call of GetDataStreaming() that should be stored
                        // in the next wavefrom from the next call of
                        // GetDataStreaming()
                        remaining_samples[channel] = added_samples[channel] - samples_offset;
                    } else {
                        remaining_samples[channel] = 0;
                    }

                    // Copy the incomplete header to the start of the target_headers buffer
                    memcpy(target_headers[channel], target_headers[channel] + (added_headers[channel]), sizeof(StreamingHeader_t));
                }
            }
        }
    } else {
        int64_t available_bytes = ADQ_EAGAIN;

        do {
            int available_channel = ADQ_ANY_CHANNEL;
            struct ADQRecord *ADQ_record;
            struct ADQDataReadoutStatus ADQ_status;

            // Using a zero here should make the function return immediately
            const int timeout = 0;

            available_bytes = ADQ_WaitForRecordBuffer(adq_cu_ptr, adq_num,
                                                                  &available_channel,
                                                                  reinterpret_cast<void**>(&ADQ_record),
                                                                  timeout,
                                                                  &ADQ_status);

            if (available_bytes == 0) {
                // This is a status only record, I am not sure how to handle it
                if (GetVerbosity() > 1)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::GetWaveformsFromCard() ";
                    std::cout << "Status only record: flags: ";
                    printf("%08" PRIu32 "", ADQ_status.flags);
                    std::cout << "; ";
                    std::cout << "Available channel: " << available_channel << "; ";
                    std::cout << std::endl;
                }
            } else if (available_bytes == ADQ_EAGAIN) {
                // This is a safe timeout, it means that data is not yet
                // available
                if (GetVerbosity() > 1)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::GetWaveformsFromCard() ";
                    std::cout << "Timeout; ";
                    std::cout << std::endl;
                }
            } else if (available_bytes < 0 && available_bytes != ADQ_EAGAIN) {
                // This is an error!
                // The record should not have been allocated and the docs
                // suggest to stop the acquisition to understand the error value
                if (GetVerbosity() > 1)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::GetWaveformsFromCard() ";
                    std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Error code: " << (long)available_bytes << "; ";
                    std::cout << std::endl;
                }
                return DIGITIZER_FAILURE;

            } else if (available_bytes > 0) {
                if (GetVerbosity() > 1)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::GetWaveformsFromCard() ";
                    std::cout << "Available data; ";
                    std::cout << "Available bytes: " << (long)available_bytes << "; ";
                    std::cout << std::endl;
                }

                // At this point the ADQ_record variable should be ready...
                const uint8_t channel = ADQ_record->header->Channel;
                const uint32_t record_number = ADQ_record->header->RecordNumber;
                const uint32_t samples_per_record = ADQ_record->header->RecordLength;
                const uint64_t timestamp = ADQ_record->header->Timestamp;
                const int16_t *data_buffer = reinterpret_cast<int16_t*>(ADQ_record->data);

                if (GetVerbosity() > 1)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::GetWaveformsFromCard() ";
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

                if (timestamp_negative_difference > ADQ14_FWPD_TIMESTAMP_THRESHOLD) {
                    if (GetVerbosity() > 0)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::GetWaveformsFromCard() ";
                        std::cout << WRITE_YELLOW << "WARNING" << WRITE_NC << ": Detected timestamp overflow; ";
                        std::cout << "Overflows: " << timestamp_overflows << "; ";
                        std::cout << "Negative difference: " << (long long)timestamp_negative_difference << "; ";
                        std::cout << std::endl;
                    }

                    timestamp_offset += ADQ14_FWPD_TIMESTAMP_MAX;
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

                for (unsigned int sample_index = 0; sample_index < samples_per_record; sample_index++) {
                    const int16_t value = data_buffer[sample_index];

                    // We add an offset to match it to the rest of ABCD
                    // The 14 bit data is aligned to the MSB thus we should push it back
                    samples[sample_index] = ((value >> 2) + (1 << 15));
                }

                waveforms.push_back(this_waveform);

                CHECKZERO(ADQ_ReturnRecordBuffer(adq_cu_ptr, adq_num,
                                                             available_channel,
                                                             ADQ_record));
            }

        } while (available_bytes >= 0);

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::GetWaveformsFromCard() ";
            std::cout << "Converted all samples; ";
            std::cout << "Timestamp overflows: " << timestamp_overflows << "; ";
            std::cout << std::endl;
        }
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWPD::StopAcquisition()
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::StopAcquisition() ";
        std::cout << "Using streaming generation: " << streaming_generation << "; ";
        std::cout << std::endl;
    }

    if (streaming_generation == 1) {
        CHECKZERO(ADQ_StopStreaming(adq_cu_ptr, adq_num));
    } else {
        const int retval = ADQ_StopDataAcquisition(adq_cu_ptr, adq_num);

        if (retval != ADQ_EOK && retval != ADQ_EINTERRUPTED) {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::StopAcquisition() ";
            std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Stop acquisition error ";
            std::cout << " (code: " << WRITE_YELLOW << retval << WRITE_NC << "); ";
            std::cout << std::endl;
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::StopAcquisition() ";
            std::cout << WRITE_RED << ADQ_descriptions::error.at(retval) << WRITE_NC << "; ";
            std::cout << std::endl;

            return DIGITIZER_FAILURE;
        }
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWPD::ForceSoftwareTrigger()
{
    if (GetVerbosity() > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ForceSoftwareTrigger() ";
        std::cout << "Forcing a software trigger; ";
        std::cout << std::endl;
    }

    CHECKZERO(ADQ_SWTrig(adq_cu_ptr, adq_num));

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWPD::ReadConfig(json_t *config)
{
    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Wrong clock source";
        std::cout << std::endl;
    }

    json_object_set_nocheck(config, "clock_source", json_string(ADQ_descriptions::clock_source.at(clock_source).c_str()));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
        std::cout << "Clock source: got: " << ADQ_descriptions::clock_source.at(clock_source) << " (index: " << clock_source << "); ";
        std::cout << std::endl;
    }

    timestamp_bit_shift = json_number_value(json_object_get(config, "timestamp_bit_shift"));

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
        std::cout << "Timestamp bit shift: " << timestamp_bit_shift << " bits; ";
        std::cout << std::endl;
    }

    json_object_set_nocheck(config, "timestamp_bit_shift", json_integer(timestamp_bit_shift));

    // -------------------------------------------------------------------------
    //  Reading the transfer configuration
    // -------------------------------------------------------------------------
    json_t *transfer_config = json_object_get(config, "transfer");

    if (!json_is_object(transfer_config)) {
        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
            std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Missing \"transfer\" entry in configuration; ";
            std::cout << std::endl;
        }

        transfer_config = json_object();
        json_object_set_new_nocheck(config, "transfer", transfer_config);
    }

    const unsigned int raw_buffer_size = json_number_value(json_object_get(transfer_config, "buffer_size"));

    transfer_buffer_size = ceil(raw_buffer_size < 1 ? 1 : raw_buffer_size) * 1024;

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
        std::cout << "Transfer buffer size: " << transfer_buffer_size << " B, " << transfer_buffer_size / 1024 << " kiB; ";
        std::cout << std::endl;
    }

    json_object_set_nocheck(transfer_config, "buffer_size", json_integer(ceil(raw_buffer_size)));

    const unsigned int raw_buffers_number = json_number_value(json_object_get(transfer_config, "buffers_number"));

    transfer_buffers_number = (raw_buffers_number < 2) ? 8 : raw_buffers_number;

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
        std::cout << "Transfer buffers number: " << transfer_buffers_number << "; ";
        std::cout << std::endl;
    }

    json_object_set_nocheck(transfer_config, "buffers_number", json_integer(transfer_buffers_number));

    const unsigned int raw_timeout = json_number_value(json_object_get(transfer_config, "timeout"));

    transfer_timeout = (raw_timeout <= 0) ? 60000 : raw_timeout;

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
        std::cout << "Transfer timeout: " << transfer_timeout << " ms; ";
        std::cout << std::endl;
    }

    json_object_set_nocheck(transfer_config, "timeout", json_integer(transfer_timeout));

    DMA_flush_timeout = json_number_value(json_object_get(transfer_config, "DMA_flush_timeout"));

    if (DMA_flush_timeout <= 0) {
        DMA_flush_timeout = default_DMA_flush_timeout;
    }

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
        std::cout << "DMA flush timeout: " << DMA_flush_timeout << " ms; ";
        std::cout << std::endl;
    }

    json_object_set_nocheck(transfer_config, "DMA_flush_timeout", json_integer(DMA_flush_timeout));

    // -------------------------------------------------------------------------
    //  Reading the trigger configuration
    // -------------------------------------------------------------------------
    json_t *trigger_config = json_object_get(config, "trigger");

    if (!json_is_object(trigger_config)) {
        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
        std::cout << "Trigger source: " << str_trigger_source<< "; ";
        std::cout << std::endl;
    }

    // Looking for the settings in the description map
    const auto tm_result = map_utilities::find_item(ADQ_descriptions::trig_mode, str_trigger_source);

    if (tm_result != ADQ_descriptions::trig_mode.end() && str_trigger_source.length() > 0) {
        trig_mode = tm_result->first;
    } else {
        trig_mode = ADQ_LEVEL_TRIGGER_MODE;

        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Invalid trigger source; ";
        std::cout << "Got: " << str_trigger_source << "; ";
        std::cout << std::endl;
    }

    json_object_set_nocheck(trigger_config, "source", json_string(ADQ_descriptions::trig_mode.at(trig_mode).c_str()));

    const char *cstr_trigger_impedance = json_string_value(json_object_get(trigger_config, "impedance"));
    const std::string str_trigger_impedance = (cstr_trigger_impedance) ? std::string(cstr_trigger_impedance) : std::string();

    if (GetVerbosity() > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Invalid trigger impedance; ";
        std::cout << "Got: " << str_trigger_impedance << "; ";
        std::cout << std::endl;
    }

    json_object_set_nocheck(trigger_config, "impedance", json_string(ADQ_descriptions::input_impedance.at(trig_port_input_impedance).c_str()));

    // -------------------------------------------------------------------------
    //  Reading the sync configuration
    // -------------------------------------------------------------------------
    json_t *sync_config = json_object_get(config, "sync");

    if (!json_is_object(sync_config)) {
        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::Configure() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
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
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
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
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
                    std::cout << "Found channel: " << id << "; ";
                    std::cout << std::endl;
                }

                const bool enabled = json_is_true(json_object_get(value, "enable"));

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
                    std::cout << "Channel is " << (enabled ? "enabled" : "disabled") << "; ";
                    std::cout << std::endl;
                }

                float input_range = json_number_value(json_object_get(value, "input_range"));

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
                    std::cout << "Input range: " << input_range << " mVpp; ";
                    std::cout << std::endl;
                }

                if (input_range <= 0) {
                    input_range = default_input_range;
                }

                const int DC_offset = json_number_value(json_object_get(value, "DC_offset"));

                json_object_set_nocheck(value, "DC_offset", json_integer(DC_offset));

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
                    std::cout << "DC offset: " << DC_offset << " samples; ";
                    std::cout << std::endl;
                }

                const bool DBS_disable = json_is_true(json_object_get(value, "DBS_disable"));

                json_object_set_nocheck(value, "DBS_disable", DBS_disable ? json_true() : json_false());

                if (GetVerbosity() > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
                    std::cout << "DBS disable: " << (DBS_disable ? "true" : "false") << "; ";
                    std::cout << std::endl;
                }

                // This level should be the relative to the baseline in the FWPD
                const int trig_level = json_number_value(json_object_get(value, "trigger_level"));

                json_object_set_nocheck(value, "trigger_level", json_integer(trig_level));

                if (GetVerbosity() > 0) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
                    std::cout << "Trigger level: " << trig_level << "; ";
                    std::cout << std::endl;
                }

                const int trig_hysteresis = json_number_value(json_object_get(value, "trigger_hysteresis"));

                json_object_set_nocheck(value, "trigger_hysteresis", json_integer(trig_hysteresis));

                if (GetVerbosity() > 0) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
                    std::cout << "Trigger hysteresis: " << trig_hysteresis << "; ";
                    std::cout << std::endl;
                }

                const char *cstr_trigger_slope = json_string_value(json_object_get(value, "trigger_slope"));
                const std::string str_trigger_slope = (cstr_trigger_slope) ? std::string(cstr_trigger_slope) : std::string();

                if (GetVerbosity() > 0) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
                    std::cout << "Trigger slope: " << str_trigger_slope<< "; ";
                    std::cout << std::endl;
                }

                // Looking for the settings in the description map
                const auto ts_result = map_utilities::find_item(ADQ_descriptions::trig_slope, str_trigger_slope);

                if (ts_result != ADQ_descriptions::trig_slope.end() && str_trigger_slope.length() > 0) {
                    trig_slope = ts_result->first;

                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
                    std::cout << "Found matching trigger slope; ";
                    std::cout << "Got: " << str_trigger_slope << "; ";
                    std::cout << "index: " << trig_slope << "; ";
                    std::cout << std::endl;
                } else {
                    trig_slope = ADQ_TRIG_SLOPE_FALLING;

                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
                    std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Invalid trigger slope; ";
                    std::cout << "Got: " << str_trigger_slope << "; ";
                    std::cout << std::endl;
                }

                json_object_set_nocheck(value, "trigger_slope", json_string(ADQ_descriptions::trig_slope.at(trig_slope).c_str()));

                const int pretrigger = json_number_value(json_object_get(value, "pretrigger"));

                json_object_set_nocheck(value, "pretrigger", json_integer(pretrigger));

                if (GetVerbosity() > 0) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
                    std::cout << "Pretrigger: " << pretrigger << "; ";
                    std::cout << std::endl;
                }

                int this_smooth_samples = json_number_value(json_object_get(value, "smooth_samples"));
                this_smooth_samples = (0 <= this_smooth_samples && this_smooth_samples < ADQ_ADQ14_MAX_SMOOTH_SAMPLES) ? this_smooth_samples : ADQ_ADQ14_MAX_SMOOTH_SAMPLES;

                json_object_set_nocheck(value, "smooth_samples", json_integer(this_smooth_samples));

                if (GetVerbosity() > 0) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
                    std::cout << "Smooth samples: " << this_smooth_samples << "; ";
                    std::cout << std::endl;
                }

                int smooth_delay = json_number_value(json_object_get(value, "smooth_delay"));
                smooth_delay = (0 <= smooth_delay && smooth_delay < ADQ_ADQ14_MAX_SMOOTH_SAMPLES) ? smooth_delay : 0;
                smooth_delay = ((smooth_delay + this_smooth_samples) < ADQ_ADQ14_MAX_SMOOTH_SAMPLES) ? smooth_delay : (ADQ_ADQ14_MAX_SMOOTH_SAMPLES - this_smooth_samples);

                json_object_set_nocheck(value, "smooth_delay", json_integer(smooth_delay));

                if (GetVerbosity() > 0) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
                    std::cout << "Smooth delay: " << smooth_delay << "; ";
                    std::cout << std::endl;
                }
                const unsigned int scope = json_number_value(json_object_get(value, "scope_samples"));

                json_object_set_nocheck(value, "scope_samples", json_integer(scope));

                if (GetVerbosity() > 0) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
                    std::cout << "Scope samples: " << scope << "; ";
                    std::cout << std::endl;
                }

                const int raw_records = json_number_value(json_object_get(value, "records_number"));

                const int records = (raw_records < 1) ? ADQ_INFINITE_NOF_RECORDS : raw_records;

                json_object_set_nocheck(value, "records_number", json_integer(records));

                if (GetVerbosity() > 0) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
                    std::cout << "Records number: " << records << "; ";
                    std::cout << std::endl;
                }

                if (0 <= id && id < static_cast<int>(GetChannelsNumber())) {
                    SetChannelEnabled(id, enabled);
                    desired_input_ranges[id] = input_range;
                    trig_levels[id] = trig_level;
                    trig_hysteresises[id] = trig_hysteresis;
                    trig_slopes[id] = trig_slope;
                    pretriggers[id] = pretrigger;
                    DC_offsets[id] = DC_offset;
                    DBS_disableds[id] = DBS_disable;
                    smooth_samples[id] = this_smooth_samples;
                    smooth_delays[id] = smooth_delay;
                    scope_samples[id] = scope;
                    records_numbers[id] = records;
                } else {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::ReadConfig() ";
                    std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Channel out of range, ignoring it; ";
                    std::cout << std::endl;
                }
            }
        }
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================

int ABCD::ADQ14_FWPD::SpecificCommand(json_t *json_command)
{
    const char *cstr_command = json_string_value(json_object_get(json_command, "command"));
    const std::string command = (cstr_command) ? std::string(cstr_command) : std::string();

    if (GetVerbosity() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::SpecificCommand() ";
        std::cout << "Specific command: " << command << "; ";
        std::cout << std::endl;
    }

    if (command == std::string("GPIO_set_direction")) {
        const int port = json_number_value(json_object_get(json_command, "port"));
        const int direction = json_number_value(json_object_get(json_command, "direction"));
        const int mask = json_number_value(json_object_get(json_command, "mask"));

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::SpecificCommand() ";
            std::cout << "Setting GPIO direction: " << direction << "; ";
            std::cout << "mask: " << mask << "; ";
            std::cout << std::endl;
        }

        CHECKZERO(ADQ_EnableGPIOPort(adq_cu_ptr, adq_num, port, 1));
        CHECKZERO(ADQ_SetDirectionGPIOPort(adq_cu_ptr, adq_num, port, direction, mask));

    } else if (command == std::string("GPIO_pulse")) {
        const int port = json_integer_value(json_object_get(json_command, "port"));
        const int width = json_integer_value(json_object_get(json_command, "width"));
        const int mask = json_number_value(json_object_get(json_command, "mask"));

        if (GetVerbosity() > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::SpecificCommand() ";
            std::cout << "Pulse on GPIO of width: " << width << " us; ";
            std::cout << std::endl;
        }

        const uint16_t pin_value_on = 1;
        const uint16_t pin_value_off = 0;

        CHECKZERO(ADQ_WriteGPIOPort(adq_cu_ptr, adq_num, port, pin_value_on, mask));

        std::this_thread::sleep_for(std::chrono::microseconds(width));

        CHECKZERO(ADQ_WriteGPIOPort(adq_cu_ptr, adq_num, port, pin_value_off, mask));
    } else if (command == std::string("timestamp_reset")) {
        // ---------------------------------------------------------------------
        //  Timestamp reset
        // ---------------------------------------------------------------------

        const bool timestamp_reset_arm = json_is_true(json_object_get(json_command, "arm"));

        if (timestamp_reset_arm) {
            // -----------------------------------------------------------------
            //  Arming the timestamp reset
            // -----------------------------------------------------------------
            unsigned int timestamp_reset_mode = ADQ_TIMESTAMP_SYNCHRONIZATION_MODE_FIRST;

            const char *cstr_timestamp_reset_mode = json_string_value(json_object_get(json_command, "mode"));
            const std::string str_timestamp_reset_mode = (cstr_timestamp_reset_mode) ? std::string(cstr_timestamp_reset_mode) : std::string();

            if (GetVerbosity() > 0) {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::SpecificCommand() ";
                std::cout << "Timestamp reset mode: " << str_timestamp_reset_mode<< "; ";
                std::cout << std::endl;
            }

            // Looking for the settings in the description map
            const auto tsm_result = map_utilities::find_item(ADQ_descriptions::timestamp_synchronization_mode, str_timestamp_reset_mode);

            if (tsm_result != ADQ_descriptions::timestamp_synchronization_mode.end() && str_timestamp_reset_mode.length() > 0) {
                timestamp_reset_mode = tsm_result->first;
            } else {
                timestamp_reset_mode = ADQ_TIMESTAMP_SYNCHRONIZATION_MODE_FIRST;

                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::SpecificCommand() ";
                std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Invalid timestamp_reset mode; ";
                std::cout << "Got: " << str_timestamp_reset_mode << "; ";
                std::cout << std::endl;
            }

            unsigned int timestamp_reset_source = ADQ_EVENT_SOURCE_SYNC;

            const char *cstr_timestamp_reset_source = json_string_value(json_object_get(json_command, "source"));
            const std::string str_timestamp_reset_source = (cstr_timestamp_reset_source) ? std::string(cstr_timestamp_reset_source) : std::string();

            if (GetVerbosity() > 0) {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::SpecificCommand() ";
                std::cout << "Timestamp synchronization source: " << str_timestamp_reset_source<< "; ";
                std::cout << std::endl;
            }

            // Looking for the settings in the description map
            const auto tss_result = map_utilities::find_item(ADQ_descriptions::timestamp_synchronization_source, str_timestamp_reset_source);

            if (tss_result != ADQ_descriptions::timestamp_synchronization_source.end() && str_timestamp_reset_source.length() > 0) {
                timestamp_reset_source = tss_result->first;
            } else {
                timestamp_reset_source = ADQ_EVENT_SOURCE_SYNC;

                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::SpecificCommand() ";
                std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Invalid timestamp_reset source; ";
                std::cout << "Got: " << str_timestamp_reset_source << "; ";
                std::cout << std::endl;
            }

            if (GetVerbosity() > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::SpecificCommand() ";
                std::cout << "Arming timestamp synchronization; ";
                std::cout << "mode: " << ADQ_descriptions::timestamp_synchronization_mode.at(timestamp_reset_mode) << "; ";
                std::cout << "source: " << ADQ_descriptions::timestamp_synchronization_source.at(timestamp_reset_source) << "; ";
                std::cout << std::endl;
            }

            CHECKZERO(ADQ_DisarmTimestampSync(adq_cu_ptr, adq_num));
            CHECKZERO(ADQ_SetupTimestampSync(adq_cu_ptr, adq_num, timestamp_reset_mode, timestamp_reset_source));
            CHECKZERO(ADQ_ArmTimestampSync(adq_cu_ptr, adq_num));

        } else {
            // -----------------------------------------------------------------
            //  Disarming the timestamp reset
            // -----------------------------------------------------------------
            if (GetVerbosity() > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ABCD::ADQ14_FWPD::SpecificCommand() ";
                std::cout << "Disarming timestamp reset; ";
                std::cout << std::endl;
            }

            CHECKZERO(ADQ_DisarmTimestampSync(adq_cu_ptr, adq_num));
        }
    }

    return DIGITIZER_SUCCESS;
}

//==============================================================================
