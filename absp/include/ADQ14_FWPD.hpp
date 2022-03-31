#ifndef __ADQ14_FWPD_HPP__
#define __ADQ14_FWPD_HPP__

#include <map>
#include <chrono>

extern "C" {
#include <jansson.h>
#include "streaming_header.h"
}

#include "Digitizer.hpp"

#define ADQ14_FWPD_MAX_CHANNELS_NUMBER 8

#define ADQ14_FWPD_RECORD_HEADER_SIZE 40
#define ADQ14_FWPD_RECORD_HEADER_MASK_LOST_RECORD (1 << 0)
#define ADQ14_FWPD_RECORD_HEADER_MASK_LOST_DATA ((1 << 1) + (1 << 2) + (1 << 3))
#define ADQ14_FWPD_RECORD_HEADER_MASK_FIFO_FILL ((1 << 4) + (1 << 5) + (1 << 6))
#define ADQ14_FWPD_RECORD_HEADER_MASK_OVER_RANGE (1 << 7)

#define ADQ14_FWPD_TIMESTAMP_BITS 63
#define ADQ14_FWPD_TIMESTAMP_MAX (1UL << ADQ14_FWDAQ_TIMESTAMP_BITS)
#define ADQ14_FWPD_TIMESTAMP_THRESHOLD (1L << (ADQ14_FWDAQ_TIMESTAMP_BITS - 1))

class ADQ14_FWPD : public Digitizer {
private:
    // Putting this to notify the compiler that we do intend to replace the
    // method but we do not want the user to call the base method.
    using Digitizer::Initialize;

public:
    // Descriptions of the flag values
    static const std::map<int, std::string> description_errors;
    static const std::map<unsigned int, std::string> description_clock_source;
    static const std::map<unsigned int, std::string> description_trig_mode;
    static const std::map<unsigned int, std::string> description_trig_slope;
    static const std::map<unsigned int, std::string> description_collection_mode;

    // Pointer to the control unit of the ADQ cards
    void* adq_cu_ptr;

    // Number of the ADQ14_FWPD card 
    int adq14_num;

    // Variable storing the generation of the firmware to disable some features
    unsigned int FWPD_generation;
    int streaming_generation;

    // Enable channels self triggering flag
    // TODO: Check if this works
    std::vector<bool> channels_triggering_enabled;

    static const double default_trig_ext_threshold;
    static const unsigned int default_trig_ext_slope;

    // The desired input ranges as requested by the user
    static const float default_input_range;
    std::vector<float> desired_input_ranges;

    // The desired input ranges as requested by the user
    static const int default_DC_offset;
    std::vector<int16_t> DC_offsets;

    // Digital Baseline Stabilization settings
    unsigned int DBS_instances_number;
    static const int default_DBS_target;
    static const int default_DBS_saturation_level_lower;
    static const int default_DBS_saturation_level_upper;
    std::vector<bool> DBS_disableds;

    std::vector<uint32_t> baseline_samples;
    std::vector<uint32_t> scope_samples;
    std::vector<int> records_numbers;

    int64_t transfer_buffer_size;
    unsigned int transfer_buffers_number;
    unsigned int transfer_timeout;

    struct ADQDataReadoutParameters readout_parameters;

    // Flag to select the clock source of the digitizer
    int clock_source;

    // Number of samples to acquire in the waveforms before the trigger
    std::vector<int32_t> pretriggers;
    std::vector<int16_t> trig_levels;
    std::vector<int> trig_slopes;

    // Flag to select the trigger mode
    unsigned int trig_mode;
    // Flag to select the trigger slope
    unsigned int trig_slope;
    // Value to change a delay of the external trigger, not used
    int trig_external_delay;
    // Value of the trigger level for the channels, in ADC samples

    // Variables to keep track of timestamp overflows during acquisitions
    int64_t timestamp_last;
    // Accumulated offset in the timestamps due to overflows
    uint64_t timestamp_offset;
    // Counter of overflows, used only for debugging
    unsigned int timestamp_overflows;

    // The timeout between buffer reads in ms
    static const unsigned int DMA_flush_timeout;
    std::chrono::time_point<std::chrono::system_clock> last_buffer_ready;

    // These variables are used to propagate state between calls of data
    // transfer APIs
    unsigned char channel_mask;
    std::vector<int16_t*> target_buffers;
    std::vector<StreamingHeader_t*> target_headers;
    std::vector<unsigned int> added_samples;
    std::vector<unsigned int> added_headers;
    std::vector<unsigned int> status_headers;
    int available_channel;
    int64_t available_bytes;
    struct ADQRecord *ADQ_record;
    struct ADQDataReadoutStatus ADQ_status;
    // This variable notifies whether the data buffers were acquired by
    // the program and need to be returned to the APIs
    bool available_buffer;

    ADQ14_FWPD(int verbosity = 0);
    ~ADQ14_FWPD();

    int Initialize(void* adq_cu_ptr, int adq14_num);
    int ReadConfig(json_t* config);
    int Configure();

    void SetChannelsNumber(unsigned int n);

    int StartAcquisition();
    int RearmTrigger();
    int StopAcquisition();
    int ForceSoftwareTrigger();

    bool AcquisitionReady();

    int GetWaveformsFromCard(std::vector<struct event_waveform> &waveforms);

    void SetDBSInstancesNumber(unsigned int n) { DBS_instances_number = n; }
    unsigned int GetDBSInstancesNumber() const { return DBS_instances_number; }
};

#endif
