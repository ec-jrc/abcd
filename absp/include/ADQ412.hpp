#ifndef __ADQ412_HPP__
#define __ADQ412_HPP__

#include <map>

extern "C" {
#include <jansson.h>
}

#include "Digitizer.hpp"

#define ADQ412_MAX_CHANNELS_NUMBER 8

#define ADQ412_RECORD_HEADER_SIZE 32

#define ADQ412_TIMESTAMP_BITS 43
#define ADQ412_TIMESTAMP_MAX (1UL << ADQ412_TIMESTAMP_BITS)
#define ADQ412_TIMESTAMP_THRESHOLD (1L << (ADQ412_TIMESTAMP_BITS - 1))

namespace ABCD {

class ADQ412 : public ABCD::Digitizer {
private:
    // Putting this to notify the compiler that we do intend to replace the
    // method but we do not want the user to call the base method.
    using ABCD::Digitizer::Initialize;

public:
    //--------------------------------------------------------------------------
    // Card settings
    //--------------------------------------------------------------------------

    // Pointer to the control unit of the ADQ cards
    void* adq_cu_ptr;

    // Number of the ADQ412 card
    int adq_num;

    // Flag to select the clock source of the digitizer
    int clock_source;

    // TODO: Understand how this works
    int pll_divider;

    //--------------------------------------------------------------------------
    // Trigger settings
    //--------------------------------------------------------------------------

    // Flag to select the trigger mode
    unsigned int trig_mode;
    // Flag to select the trigger slope
    unsigned int trig_slope;
    // Value to change a delay of the external trigger, not used
    int trig_external_delay;
    // Value of the trigger level for the channels, in ADC samples
    int trig_level;

    // Enable channels self triggering flag
    int channels_triggering_mask;

    //--------------------------------------------------------------------------
    // Channels settings
    //--------------------------------------------------------------------------

    //--------------------------------------------------------------------------
    // Waveforms settings
    //--------------------------------------------------------------------------

    // Number of samples to acquire in the waveforms before the trigger
    int32_t pretrigger;
    // Number of ADC samples per waveform
    uint32_t samples_per_record;
    // Number of waveforms per channel to store in the digitizer buffer
    unsigned int records_number;

    //--------------------------------------------------------------------------
    // Transfer settings
    //--------------------------------------------------------------------------
    // The size of the buffers in terms of int16_t samples. Each data buffer
    // must contain enough samples to store all the records consecutively.
    size_t buffers_size;

    // Create a pointer array containing the data buffer pointers
    // GetData allows for a digitizer with max 8 channels,
    // the unused pointers should be null pointers
    void* target_buffers[ADQ412_MAX_CHANNELS_NUMBER];
    std::vector<int16_t> buffers[ADQ412_MAX_CHANNELS_NUMBER];
    std::vector<uint8_t> target_headers;
    std::vector<int64_t> target_timestamps;

    //--------------------------------------------------------------------------
    // Streaming settings
    //--------------------------------------------------------------------------
    unsigned int channels_acquisition_mask;

    //--------------------------------------------------------------------------
    // Timestamps settings
    //--------------------------------------------------------------------------

    // Variables to keep track of timestamp overflows during acquisitions
    int64_t timestamp_last;
    // Accumulated offset in the timestamps due to overflows
    uint64_t timestamp_offset;
    // Counter of overflows, used only for debugging
    unsigned int timestamp_overflows;


    ADQ412(int verbosity = 0);
    virtual ~ADQ412();

    int Initialize(void* adq_cu_ptr, int adq_num);
    int ReadConfig(json_t* config);
    int Configure();

    int StartAcquisition();
    int RearmTrigger();
    int StopAcquisition();
    int ForceSoftwareTrigger();
    int ResetOverflow();

    bool AcquisitionReady();
    bool DataOverflow();

    int GetWaveformsFromCard(std::vector<struct event_waveform> &waveforms);
};
}

#endif
