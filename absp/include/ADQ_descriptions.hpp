#ifndef __ADQ_DESCRIPTIONS_HPP__
#define __ADQ_DESCRIPTIONS_HPP__

#include <map>

#define LINUX
#include "ADQAPI.h"

// These are not defined in the ADQAPI.h header

// Soft reset, restores to default power-on state
#define RESET_POWER_ON 2
// Soft reset of communication link
#define RESET_COMMUNICATION 8
// Reset of the digital data path in the ADC
#define RESET_ADC_DATA 100
// This reset level is used after a data overflow, but the documentation does
// not precisely describe it
#define RESET_OVERFLOW 4

#define ADQ_ANALOG_FRONT_END_COUPLING_AC 0
#define ADQ_ANALOG_FRONT_END_COUPLING_DC 1

#define ADQ_TRIG_SLOPE_RISING 1
#define ADQ_TRIG_SLOPE_FALLING 0

#define ADQ_TRIG_BLOCKING_ONCE 0
#define ADQ_TRIG_BLOCKING_WINDOW 1
#define ADQ_TRIG_BLOCKING_GATE 2
#define ADQ_TRIG_BLOCKING_INVERSE_WINDOW 3
#define ADQ_TRIG_BLOCKING_DISABLE 4

#define ADQ_TIMESTAMP_SYNC_FIRST 0
#define ADQ_TIMESTAMP_SYNC_ALL 0

#define ADQ_COLLECTION_MODE_RAW 0
#define ADQ_COLLECTION_MODE_METADATA 1
#define ADQ_COLLECTION_MODE_EVERY_NTH 2
#define ADQ_COLLECTION_MODE_RAW_PAD_WINDOW 3
#define ADQ_COLLECTION_MODE_RAW_PAD 4

#define ADQ_ADQ14_TEMPERATURE_PCB 0
#define ADQ_ADQ14_TEMPERATURE_ADC1 1
#define ADQ_ADQ14_TEMPERATURE_ADC2 2
#define ADQ_ADQ14_TEMPERATURE_FPGA 3
#define ADQ_ADQ14_TEMPERATURE_DCDC2A 4
#define ADQ_ADQ14_TEMPERATURE_DCDC2B 5
#define ADQ_ADQ14_TEMPERATURE_DCDC1 6

#define ADQ_ADQ412_TEMPERATURE_ADC0 1
#define ADQ_ADQ412_TEMPERATURE_ADC1 2
#define ADQ_ADQ412_TEMPERATURE_FPGA 3
#define ADQ_ADQ412_TEMPERATURE_PCB 4

#define ADQ_ADQ214_TEMPERATURE_ADC0 1
#define ADQ_ADQ214_TEMPERATURE_ADC1 2
#define ADQ_ADQ214_TEMPERATURE_FPGA 3
#define ADQ_ADQ214_TEMPERATURE_PCB 4

class ADQ_descriptions {
public:
    // Descriptions of the flag values
    static const std::map<int, std::string> error;
    static const std::map<unsigned int, std::string> clock_source;
    static const std::map<unsigned int, std::string> trig_mode;
    static const std::map<unsigned int, std::string> trig_slope;
    static const std::map<unsigned int, std::string> collection_mode;
    static const std::map<unsigned int, std::string> analog_front_end_coupling;
    static const std::map<unsigned int, std::string> ADQ14_temperatures;
    static const std::map<unsigned int, std::string> ADQ412_temperatures;
    static const std::map<unsigned int, std::string> ADQ214_temperatures;
};

#endif