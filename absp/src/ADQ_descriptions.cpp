#include <map>

#define LINUX
#include "ADQAPI.h"
#include "ADQ_descriptions.hpp"

const std::map<int, std::string> ADQ_descriptions::error = {
    {ADQ_EOK, "OK"},
    {ADQ_EINVAL, "Invalid argument"},
    {ADQ_EAGAIN, "Resource temporary unavailable"},
    {ADQ_EOVERFLOW, "Overflow"},
    {ADQ_ENOTREADY, "Resource not ready"},
    {ADQ_EINTERRUPTED, "Operation interrupted"},
    {ADQ_EIO, "I/O error"},
    {ADQ_EEXTERNAL, "External error, not from ADQAPI"},
    {ADQ_EUNSUPPORTED, "Operation not supported by device"},
    {ADQ_EINTERNAL, "Internal error, cannot be addressed by user"}
};

const std::map<unsigned int, std::string> ADQ_descriptions::clock_source = {
    {ADQ_CLOCK_INT_INTREF, "internal"},
    {ADQ_CLOCK_INT_EXTREF, "external_10MHz"},
    {ADQ_CLOCK_EXT, "external"},
    {ADQ_CLOCK_INT_PXIREF, "external_PXI"}
};

const std::map<unsigned int, std::string> ADQ_descriptions::trig_mode = {
    {ADQ_SW_TRIGGER_MODE, "software"},
    {ADQ_EXT_TRIGGER_MODE, "external"},
    {ADQ_LEVEL_TRIGGER_MODE, "channels"},
    {ADQ_INTERNAL_TRIGGER_MODE, "internal"},
    {0, "none"}
};

const std::map<unsigned int, std::string> ADQ_descriptions::trig_slope = {
    {ADQ_TRIG_SLOPE_FALLING, "falling"},
    {ADQ_TRIG_SLOPE_RISING, "rising"}
};

const std::map<unsigned int, std::string> ADQ_descriptions::collection_mode = {
    {ADQ_COLLECTION_MODE_RAW, "raw_pulse"},
    {ADQ_COLLECTION_MODE_METADATA, "pulse_metadata"},
    {ADQ_COLLECTION_MODE_EVERY_NTH, "every_nth"},
    {ADQ_COLLECTION_MODE_RAW_PAD_WINDOW, "raw_pad_window"},
    {ADQ_COLLECTION_MODE_RAW_PAD, "raw_pad"}
};

const std::map<unsigned int, std::string> ADQ_descriptions::ADQ14_temperatures = {
    {ADQ_ADQ14_TEMPERATURE_PCB, "PCB"},
    {ADQ_ADQ14_TEMPERATURE_ADC1, "ADC1"},
    {ADQ_ADQ14_TEMPERATURE_ADC2, "ADC2"},
    {ADQ_ADQ14_TEMPERATURE_FPGA, "FPGA"},
    {ADQ_ADQ14_TEMPERATURE_DCDC2A, "DCDC2A"},
    {ADQ_ADQ14_TEMPERATURE_DCDC2B, "DCDC2B"},
    {ADQ_ADQ14_TEMPERATURE_DCDC1, "DCDC1"}
};

const std::map<unsigned int, std::string> ADQ_descriptions::ADQ412_temperatures = {
    {ADQ_ADQ412_TEMPERATURE_ADC0, "ADC0"},
    {ADQ_ADQ412_TEMPERATURE_ADC1, "ADC1"},
    {ADQ_ADQ412_TEMPERATURE_FPGA, "FPGA"},
    {ADQ_ADQ412_TEMPERATURE_PCB, "PCB"}
};


const std::map<unsigned int, std::string> ADQ_descriptions::ADQ214_temperatures = {
    {ADQ_ADQ214_TEMPERATURE_ADC0, "ADC0"},
    {ADQ_ADQ214_TEMPERATURE_ADC1, "ADC1"},
    {ADQ_ADQ214_TEMPERATURE_FPGA, "FPGA"},
    {ADQ_ADQ214_TEMPERATURE_PCB, "PCB"}
};
