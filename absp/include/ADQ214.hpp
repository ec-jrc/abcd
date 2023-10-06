#ifndef __ADQ214_HPP__
#define __ADQ214_HPP__

#include <map>

extern "C" {
#include <jansson.h>
}

#include "Digitizer.hpp"

#define ADQ214_RECORD_HEADER_SIZE 32

#define ADQ214_TIMESTAMP_BITS 42
#define ADQ214_TIMESTAMP_MAX (1UL << ADQ214_TIMESTAMP_BITS)
#define ADQ214_TIMESTAMP_THRESHOLD (1L << (ADQ214_TIMESTAMP_BITS - 1))

namespace ABCD {

class ADQ214 : public ABCD::Digitizer {
private:
    // Putting this to notify the compiler that we do intend to replace the
    // method but we do not want the user to call the base method.
    using ABCD::Digitizer::Initialize;

    // -------------------------------------------------------------------------
    //  Card settings
    // -------------------------------------------------------------------------

    // Pointer to the control unit of the ADQ cards
    void* adq_cu_ptr;

    // Number of the ADQ214 card
    unsigned int adq_num;

public:
    // Flag to select the clock source of the digitizer
    int clock_source;

    // Settings of the clock PLL divider
    int PLL_divider;

    // Settings for the trigger output
    bool trigger_output_enable;
    unsigned int trigger_output_port;
    unsigned int trigger_output_mode;
    int trigger_output_width;
    static const int default_trigger_output_width;

    // -------------------------------------------------------------------------
    //  Trigger settings
    // -------------------------------------------------------------------------

    // Flag to select the trigger mode
    unsigned int trigger_mode;
    // Flag to select the trigger slope
    unsigned int trigger_slope;
    // Value to change a delay of the external trigger, not used
    //int trigger_external_delay;

    // Value of the trigger level for the channels, in ADC samples
    int trigger_level;

    // Enable channels self triggering flag
    int channels_triggering_mask;

    // -------------------------------------------------------------------------
    //  Channels settings
    // -------------------------------------------------------------------------

    // Channels couplings in the analong front ends
    uint8_t channels_analog_front_end_mask;
    std::vector<uint8_t> analog_front_end_couplings;

    // -------------------------------------------------------------------------
    //  Waveforms settings
    // -------------------------------------------------------------------------

    // Number of samples to acquire in the waveforms before the trigger
    int32_t pretrigger;
    // Number of ADC samples per waveform
    uint32_t samples_per_record;
    // Number of waveforms per channel to store in the digitizer buffer
    unsigned int records_number;

    // -------------------------------------------------------------------------
    //  Transfer settings
    // -------------------------------------------------------------------------
    // The size of the buffers in terms of int16_t samples. Each data buffer
    // must contain enough samples to store all the records consecutively.
    size_t buffers_size;

    // Create a pointer array containing the data buffer pointers
    // GetData allows for a digitizer with max 8 channels,
    // the unused pointers should be null pointers
    void* target_buffers[ADQ_GETDATA_MAX_NOF_CHANNELS];
    std::vector<int16_t> buffers[ADQ_GETDATA_MAX_NOF_CHANNELS];
    std::vector<uint8_t> target_headers;
    std::vector<int64_t> target_timestamps;

    // -------------------------------------------------------------------------
    //  Streaming settings
    // -------------------------------------------------------------------------
    unsigned int channels_acquisition_mask;

    // -------------------------------------------------------------------------
    //  Timestamps settings
    // -------------------------------------------------------------------------

    // Variables to keep track of timestamp overflows during acquisitions
    int64_t timestamp_last;
    // Accumulated offset in the timestamps due to overflows
    uint64_t timestamp_offset;
    // Counter of overflows, used only for debugging
    unsigned int timestamp_overflows;


    ADQ214(void* adq_cu_ptr, int adq_num, int verbosity = 0);
    virtual ~ADQ214();

    int Initialize();
    int ReadConfig(json_t* config);
    int Configure();

    void SetChannelsNumber(unsigned int n);

    int StartAcquisition();
    int RearmTrigger();
    int StopAcquisition();
    int ForceSoftwareTrigger();
    int ResetOverflow();

    bool AcquisitionReady();
    bool DataOverflow();

    int GetWaveformsFromCard(std::vector<struct event_waveform> &waveforms);

    //--------------------------------------------------------------------------

    int SpecificCommand(json_t* json_command);

};
}

#endif
