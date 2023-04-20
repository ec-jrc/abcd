#ifndef __ADQ14_FWDAQ_HPP__
#define __ADQ14_FWDAQ_HPP__

#include <map>

extern "C" {
#include <jansson.h>
}

#include "Digitizer.hpp"

#define ADQ14_FWDAQ_RECORD_HEADER_SIZE 40
#define ADQ14_FWDAQ_RECORD_HEADER_MASK_LOST_RECORD (1 << 0)
#define ADQ14_FWDAQ_RECORD_HEADER_MASK_LOST_DATA ((1 << 1) + (1 << 2) + (1 << 3))
#define ADQ14_FWDAQ_RECORD_HEADER_MASK_FIFO_FILL ((1 << 4) + (1 << 5) + (1 << 6))
#define ADQ14_FWDAQ_RECORD_HEADER_MASK_OVER_RANGE (1 << 7)

#define ADQ14_FWDAQ_TIMESTAMP_BITS 63
#define ADQ14_FWDAQ_TIMESTAMP_MAX (1UL << ADQ14_FWDAQ_TIMESTAMP_BITS)
#define ADQ14_FWDAQ_TIMESTAMP_THRESHOLD (1L << (ADQ14_FWDAQ_TIMESTAMP_BITS - 1))

namespace ABCD {

class ADQ14_FWDAQ : public ABCD::Digitizer {
private:
    // Putting this to notify the compiler that we do intend to replace the
    // method but we do not want the user to call the base method.
    using ABCD::Digitizer::Initialize;

public:
    // -------------------------------------------------------------------------
    //  Card settings
    // -------------------------------------------------------------------------

    // Pointer to the control unit of the ADQ cards
    void* adq_cu_ptr;

    // Number of the ADQ14_FWDAQ card
    int adq14_num;

    // Flag to select the clock source of the digitizer
    int clock_source;

    // Settings for the input impedances of the front connectors
    int trig_port_input_impedance;
    int sync_port_input_impedance;

    // -------------------------------------------------------------------------
    //  Trigger settings
    // -------------------------------------------------------------------------

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

    // -------------------------------------------------------------------------
    //  Channels settings
    // -------------------------------------------------------------------------

    // The desired input ranges as requested by the user
    static const float default_input_range;
    std::vector<float> desired_input_ranges;

    // The hardware DC offsets set on the channels as requested by the user
    static const int default_DC_offset;
    std::vector<int16_t> DC_offsets;

    // Digital Baseline Stabilization settings
    unsigned int DBS_instances_number;
    static const int default_DBS_target;
    static const int default_DBS_saturation_level_lower;
    static const int default_DBS_saturation_level_upper;
    std::vector<bool> DBS_disableds;
    std::vector<uint32_t> baseline_samples;

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
    // Applied bit shift to the read timestamp values
    unsigned int timestamp_bit_shift;

    // Variables to keep track of timestamp overflows during acquisitions
    int64_t timestamp_last;
    // Accumulated offset in the timestamps due to overflows
    uint64_t timestamp_offset;
    // Counter of overflows, used only for debugging
    unsigned int timestamp_overflows;

    ADQ14_FWDAQ(int verbosity = 0);
    virtual ~ADQ14_FWDAQ();

    int Initialize(void* adq_cu_ptr, int adq14_num);
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

    void SetDBSInstancesNumber(unsigned int n) { DBS_instances_number = n; }
    unsigned int GetDBSInstancesNumber() const { return DBS_instances_number; }

    //--------------------------------------------------------------------------

    int SpecificCommand(json_t* json_command);
};
}

#endif
