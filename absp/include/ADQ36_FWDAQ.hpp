#ifndef __ADQ36_FWDAQ_HPP__
#define __ADQ36_FWDAQ_HPP__

#include <map>
#include <chrono>

extern "C" {
#include <jansson.h>
}

#include "Digitizer.hpp"

#define ADQ36_FWDAQ_TIMESTAMP_BITS 63
#define ADQ36_FWDAQ_TIMESTAMP_MAX (1UL << ADQ36_FWDAQ_TIMESTAMP_BITS)
#define ADQ36_FWDAQ_TIMESTAMP_THRESHOLD (1L << (ADQ36_FWDAQ_TIMESTAMP_BITS - 1))

namespace ABCD {

class ADQ36_FWDAQ : public ABCD::Digitizer {
private:
    // Putting this to notify the compiler that we do intend to replace the
    // method but we do not want the user to call the base method.
    using ABCD::Digitizer::Initialize;

    // -------------------------------------------------------------------------
    //  Card settings
    // -------------------------------------------------------------------------

    // Pointer to the control unit of the ADQ cards
    void* adq_cu_ptr;

    // Number of the ADQ36_FWDAQ card 
    int adq_num;

public:
    // Parameters that configure the digitizer
    struct ADQParameters adq_parameters;

    // Parameters in the native format of the ADQ36 in a JSON formatted string
    std::string native_config;

    // Flag to signal that at least one channels is using the software trigger
    bool using_software_trigger;

    // -------------------------------------------------------------------------
    //  Custom JRC-Geel firmware configuration
    // -------------------------------------------------------------------------
    bool custom_firmware_enabled;
    uint32_t custom_firmware_pulse_length;
    uint32_t custom_firmware_mode;

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

    ADQ36_FWDAQ(void* adq_cu_ptr, int adq14_num, int verbosity = 0);
    ~ADQ36_FWDAQ();

    int Initialize();
    int ReadConfig(json_t* config);
    int Configure();

    void SetChannelsNumber(unsigned int n);

    int StartAcquisition();
    int RearmTrigger();
    int StopAcquisition();
    int ForceSoftwareTrigger();

    bool AcquisitionReady();

    int GetWaveformsFromCard(std::vector<struct event_waveform> &waveforms);

    //--------------------------------------------------------------------------

    int SpecificCommand(json_t* json_command);

    std::string GetParametersString(enum ADQParameterId parameter_id);
    int SetParametersString(const std::string parameters);

    json_t *GetParametersJSON(enum ADQParameterId parameter_id);
    int SetParametersJSON(const json_t *parameters);

    //--------------------------------------------------------------------------

    int CustomFirmwareSetMode(uint32_t mode);
    int CustomFirmwareSetPulseLength(uint32_t pulse_length);
    int CustomFirmwareEnable(bool enable);
};
}

#endif
