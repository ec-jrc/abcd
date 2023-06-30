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
    //{ADQ_CLOCK_INT_INTREF, "internal"},
    //{ADQ_CLOCK_INT_EXTREF, "external_10MHz"},
    //{ADQ_CLOCK_EXT, "external"},
    //{ADQ_CLOCK_INT_PXIREF, "external_PXI"}
    {ADQ_CLOCK_SOURCE_INTREF, "internal"},
    {ADQ_CLOCK_SOURCE_EXTREF, "external_10MHz"},
    {ADQ_CLOCK_SOURCE_EXTCLK, "external"},
    {ADQ_CLOCK_SOURCE_PXIE_10M, "external_PXI"},
    {ADQ_CLOCK_SOURCE_PXIE_100M, "external_PXIe_100MHz"}
};

const std::map<enum ADQReferenceClockSource, std::string> ADQ_descriptions::ADQ36_clock_source = {
    {ADQ_REFERENCE_CLOCK_SOURCE_INTERNAL, "internal"},
    {ADQ_REFERENCE_CLOCK_SOURCE_PORT_CLK, "external"},
    {ADQ_REFERENCE_CLOCK_SOURCE_PXIE_10M, "external_PXI"},
    {ADQ_REFERENCE_CLOCK_SOURCE_PXIE_100M, "external_PXIe_100MHz"}
};

const std::map<enum ADQEventSource, std::string> ADQ_descriptions::ADQ36_trigger_source = {
    {ADQ_EVENT_SOURCE_SOFTWARE, "software"},
    {ADQ_EVENT_SOURCE_TRIG, "trig_port"},
    {ADQ_EVENT_SOURCE_LEVEL, "self"},
    {ADQ_EVENT_SOURCE_PERIODIC, "periodic"},
    {ADQ_EVENT_SOURCE_SYNC, "sync_port"},
    {ADQ_EVENT_SOURCE_GPIOA0, "GPIOA0"},
    {ADQ_EVENT_SOURCE_LEVEL_CHANNEL0, "channel0"},
    {ADQ_EVENT_SOURCE_LEVEL_CHANNEL1, "channel1"},
    {ADQ_EVENT_SOURCE_LEVEL_CHANNEL2, "channel2"},
    {ADQ_EVENT_SOURCE_LEVEL_CHANNEL3, "channel3"},
    {ADQ_EVENT_SOURCE_LEVEL_CHANNEL4, "channel4"},
    {ADQ_EVENT_SOURCE_LEVEL_CHANNEL5, "channel5"},
    {ADQ_EVENT_SOURCE_LEVEL_CHANNEL6, "channel6"},
    {ADQ_EVENT_SOURCE_LEVEL_CHANNEL7, "channel7"},
};

const std::map<enum ADQEdge, std::string> ADQ_descriptions::ADQ36_slope = {
    {ADQ_EDGE_FALLING, "falling"},
    {ADQ_EDGE_RISING, "rising"},
    {ADQ_EDGE_BOTH, "both"}
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

const std::map<unsigned int, std::string> ADQ_descriptions::timestamp_synchronization_mode = {
    // These settings do not seem to work
    //{ADQ_TIMESTAMP_SYNCHRONIZATION_MODE_DISABLE, "disable"},
    //{ADQ_TIMESTAMP_SYNCHRONIZATION_MODE_FIRST, "first"},
    //{ADQ_TIMESTAMP_SYNCHRONIZATION_MODE_ALL, "all"}
    {0, "first"},
    {1, "all"}
};

const std::map<unsigned int, std::string> ADQ_descriptions::timestamp_synchronization_source = {
    {ADQ_EVENT_SOURCE_SOFTWARE, "software"},
    {ADQ_EVENT_SOURCE_TRIG, "trig_port"},
    {ADQ_EVENT_SOURCE_SYNC, "sync_port"}
};

const std::map<unsigned int, std::string> ADQ_descriptions::collection_mode = {
    {ADQ_COLLECTION_MODE_RAW, "raw_pulse"},
    {ADQ_COLLECTION_MODE_METADATA, "pulse_metadata"},
    {ADQ_COLLECTION_MODE_EVERY_NTH, "every_nth"},
    {ADQ_COLLECTION_MODE_RAW_PAD_WINDOW, "raw_pad_window"},
    {ADQ_COLLECTION_MODE_RAW_PAD, "raw_pad"}
};

const std::map<unsigned int, std::string> ADQ_descriptions::analog_front_end_coupling = {
    {ADQ_ANALOG_FRONT_END_COUPLING_AC, "AC"},
    {ADQ_ANALOG_FRONT_END_COUPLING_DC, "DC"}
};

const std::map<unsigned int, std::string> ADQ_descriptions::input_impedance = {
    {ADQ_IMPEDANCE_50_OHM, "50_Ohm"},
    {ADQ_IMPEDANCE_HIGH, "high"},
    {ADQ_IMPEDANCE_100_OHM, "100_Ohm"},
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

const std::map<unsigned int, std::string> ADQ_descriptions::ADQ_firmware_revisions = {
    {12847, "ADQ214_FWDAQ"},
    {24616, "ADQ14_FWDAQ"},
    {61728, "ADQ14_FWDAQ"},
    {25201, "ADQ14_FWPD"},
    {53534, "ADQ14_FWPD"},
};

const std::map<unsigned int, std::string> ADQ_descriptions::ADQ_firmware_type = {
    {ADQ_FIRMWARE_TYPE_INVALID, "invalid"},
    {ADQ_FIRMWARE_TYPE_FWDAQ, "FWDAQ"},
    {ADQ_FIRMWARE_TYPE_FWATD, "FWATD"},
};
