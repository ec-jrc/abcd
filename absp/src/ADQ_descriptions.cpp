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
    {ADQ_CLOCK_SOURCE_PXIE_10M, "external_PXIsync"},
    {ADQ_CLOCK_SOURCE_PXIE_100M, "external_PXIe_100MHz"}
};

const std::map<enum ADQReferenceClockSource, std::string> ADQ_descriptions::ADQ36_clock_source = {
    {ADQ_REFERENCE_CLOCK_SOURCE_INTERNAL, "internal"},
    {ADQ_REFERENCE_CLOCK_SOURCE_PORT_CLK, "external"},
    {ADQ_REFERENCE_CLOCK_SOURCE_PXIE_10M, "external_PXI"},
    {ADQ_REFERENCE_CLOCK_SOURCE_PXIE_100M, "external_PXIe_100MHz"}
};

const std::map<enum ADQEventSource, std::string> ADQ_descriptions::event_source = {
    {ADQ_EVENT_SOURCE_INVALID, "invalid"},
    {ADQ_EVENT_SOURCE_SOFTWARE, "software"},
    {ADQ_EVENT_SOURCE_TRIG, "trig_port"},
    {ADQ_EVENT_SOURCE_LEVEL, "self"},
    {ADQ_EVENT_SOURCE_PERIODIC, "periodic"},
    {ADQ_EVENT_SOURCE_PXIE_STARB, "PXIe_STARB"},
    {ADQ_EVENT_SOURCE_SYNC, "sync_port"},
    {ADQ_EVENT_SOURCE_DAISY_CHAIN, "daisy_chain"},
    {ADQ_EVENT_SOURCE_GPIOA0, "GPIOA0"},
    {ADQ_EVENT_SOURCE_GPIOB0, "GPIOB0"},
    {ADQ_EVENT_SOURCE_PXIE_TRIG0, "PXIe_TRIG0"},
    {ADQ_EVENT_SOURCE_PXIE_TRIG1, "PXIe_TRIG1"},
    {ADQ_EVENT_SOURCE_LEVEL_CHANNEL0, "channel0"},
    {ADQ_EVENT_SOURCE_LEVEL_CHANNEL1, "channel1"},
    {ADQ_EVENT_SOURCE_LEVEL_CHANNEL2, "channel2"},
    {ADQ_EVENT_SOURCE_LEVEL_CHANNEL3, "channel3"},
    {ADQ_EVENT_SOURCE_LEVEL_CHANNEL4, "channel4"},
    {ADQ_EVENT_SOURCE_LEVEL_CHANNEL5, "channel5"},
    {ADQ_EVENT_SOURCE_LEVEL_CHANNEL6, "channel6"},
    {ADQ_EVENT_SOURCE_LEVEL_CHANNEL7, "channel7"},
    {ADQ_EVENT_SOURCE_REFERENCE_CLOCK, "reference_clock"},
    {ADQ_EVENT_SOURCE_MATRIX, "source_matrix"},
    {ADQ_EVENT_SOURCE_LEVEL_MATRIX, "level_matrix"},
};

const std::map<enum ADQTimestampSynchronizationMode, std::string> ADQ_descriptions::timestamp_synchronization_mode = {
    {ADQ_TIMESTAMP_SYNCHRONIZATION_MODE_DISABLE, "disable"},
    {ADQ_TIMESTAMP_SYNCHRONIZATION_MODE_FIRST, "first"},
    {ADQ_TIMESTAMP_SYNCHRONIZATION_MODE_ALL, "all"},
};

const std::map<enum ADQArm, std::string> ADQ_descriptions::arm = {
    {ADQ_ARM_IMMEDIATELY, "immediately"},
    {ADQ_ARM_AT_ACQUISITION_START, "acquisition_start"},
};

const std::map<enum ADQParameterId, std::string> ADQ_descriptions::ADQ36_port_ids = {
    {ADQ_PARAMETER_ID_PORT_TRIG, "trig"},
    {ADQ_PARAMETER_ID_PORT_SYNC, "sync"},
    {ADQ_PARAMETER_ID_PORT_CLK, "clock"},
};

const std::map<enum ADQEdge, std::string> ADQ_descriptions::slope = {
    {ADQ_EDGE_FALLING, "falling"},
    {ADQ_EDGE_RISING, "rising"},
    {ADQ_EDGE_BOTH, "both"}
};

const std::map<unsigned int, std::string> ADQ_descriptions::trigger_mode = {
    {ADQ_SW_TRIGGER_MODE, "software"},
    {ADQ_EXT_TRIGGER_MODE, "external"},
    {ADQ_LEVEL_TRIGGER_MODE, "channels"},
    {ADQ_INTERNAL_TRIGGER_MODE, "internal"},
    {0, "none"}
};

const std::map<unsigned int, std::string> ADQ_descriptions::trigger_slope = {
    {ADQ_TRIG_SLOPE_FALLING, "falling"},
    {ADQ_TRIG_SLOPE_RISING, "rising"}
};

const std::map<unsigned int, std::string> ADQ_descriptions::ADQ14_timestamp_synchronization_mode = {
    // These settings do not seem to work
    //{ADQ_TIMESTAMP_SYNCHRONIZATION_MODE_DISABLE, "disable"},
    //{ADQ_TIMESTAMP_SYNCHRONIZATION_MODE_FIRST, "first"},
    //{ADQ_TIMESTAMP_SYNCHRONIZATION_MODE_ALL, "all"}
    {0, "first"},
    {1, "all"}
};

const std::map<unsigned int, std::string> ADQ_descriptions::ADQ14_timestamp_synchronization_source = {
    {ADQ_EVENT_SOURCE_SOFTWARE, "software"},
    {ADQ_EVENT_SOURCE_TRIG, "trig_port"},
    {ADQ_EVENT_SOURCE_SYNC, "sync_port"}
};

const std::map<unsigned int, std::string> ADQ_descriptions::ADQ214_trigger_output_mode = {
    {ADQ_ADQ214_TRIGGER_OUTPUT_MODE_DISABLE, "disable"},
    {ADQ_ADQ214_TRIGGER_OUTPUT_MODE_LEVEL_TRIGGER, "channels"},
    {ADQ_ADQ214_TRIGGER_OUTPUT_MODE_TRIGGER_EVENT, "event"}
};

const std::map<unsigned int, std::string> ADQ_descriptions::ADQ14_trigger_output_port = {
    {ADQ_ADQ14_TRIGGER_OUTPUT_PORT_TRIG, "trig"},
    {ADQ_ADQ14_TRIGGER_OUTPUT_PORT_SYNC, "sync"}
};

const std::map<unsigned int, std::string> ADQ_descriptions::ADQ14_trigger_output_mode = {
    {ADQ_ADQ14_TRIGGER_OUTPUT_MODE_DISABLE, "disable"},
    {ADQ_ADQ14_TRIGGER_OUTPUT_MODE_RECORD_TRIGGER, "record"},
    {ADQ_ADQ14_TRIGGER_OUTPUT_MODE_LEVEL_TRIGGER, "channels"},
    {ADQ_ADQ14_TRIGGER_OUTPUT_MODE_TRIG_PORT, "trig_port"}
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

const std::map<enum ADQImpedance, std::string> ADQ_descriptions::input_impedance = {
    {ADQ_IMPEDANCE_50_OHM, "50_Ohm"},
    {ADQ_IMPEDANCE_HIGH, "high"},
    {ADQ_IMPEDANCE_100_OHM, "100_Ohm"},
};

const std::map<enum ADQDirection, std::string> ADQ_descriptions::pin_direction = {
    {ADQ_DIRECTION_IN, "in"},
    {ADQ_DIRECTION_OUT, "out"},
    {ADQ_DIRECTION_INOUT, "in/out"},
};

const std::map<enum ADQFunction, std::string> ADQ_descriptions::pin_function = {
    {ADQ_FUNCTION_INVALID, "disabled"},
    {ADQ_FUNCTION_PATTERN_GENERATOR0, "pattern_generator0"},
    {ADQ_FUNCTION_PATTERN_GENERATOR1, "pattern_generator1"},
    {ADQ_FUNCTION_GPIO, "gpio"},
    {ADQ_FUNCTION_PULSE_GENERATOR0, "pulse_generator0"},
    {ADQ_FUNCTION_PULSE_GENERATOR1, "pulse_generator1"},
    {ADQ_FUNCTION_PULSE_GENERATOR2, "pulse_generator2"},
    {ADQ_FUNCTION_PULSE_GENERATOR3, "pulse_generator3"},
    {ADQ_FUNCTION_TIMESTAMP_SYNCHRONIZATION, "timestamp_synchronization"},
    {ADQ_FUNCTION_USER_LOGIC, "user_logic"},
    {ADQ_FUNCTION_DAISY_CHAIN, "daisy_chain"},
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
