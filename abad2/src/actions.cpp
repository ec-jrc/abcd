/*
 * (C) Copyright 2016 Cristiano Lino Fontana
 *
 * This file is part of ABCD.
 *
 * ABCD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ABCD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ABCD.  If not, see <http://www.gnu.org/licenses/>.
 */

// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <zmq.h>

#include <jansson.h>
#include <digilent/waveforms/dwf.h>

extern "C" {
#include "defaults.h"
#include "utilities_functions.h"
#include "socket_functions.h"
#include "jansson_socket_functions.h"
}

#include "typedefs.hpp"
#include "events.hpp"
#include "states.hpp"
#include "actions.hpp"

#define BUFFER_SIZE 32

/******************************************************************************/
/* Generic actions                                                            */
/******************************************************************************/

void actions::generic::publish_events(status &global_status)
{
    const size_t waveforms_buffer_size = global_status.waveforms_buffer.size();

    if (waveforms_buffer_size > 0)
    {
        size_t total_size = 0;

        for (auto &event: global_status.waveforms_buffer)
        {
            total_size += event.size();
        }

        std::vector<uint8_t> output_buffer(total_size);

        size_t portion = 0;

        for (auto &event: global_status.waveforms_buffer)
        {
            const size_t event_size = event.size();
            const std::vector<uint8_t> event_buffer = event.serialize();

            memcpy(output_buffer.data() + portion,
                   reinterpret_cast<const void*>(event_buffer.data()),
                   event_size);

            portion += event_size;
        }

        std::string topic = defaults_abcd_data_waveforms_topic;
        topic += "_v0_s";
        topic += std::to_string(total_size);

        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Sending waveforms buffer; ";
            std::cout << "Topic: " << topic << "; ";
            std::cout << "waveforms: " << waveforms_buffer_size << "; ";
            std::cout << "buffer size: " << total_size << "; ";
            std::cout << std::endl;
        }

        const bool result = send_byte_message(global_status.data_socket,
                                              topic.c_str(),
                                              output_buffer.data(),
                                              total_size,
                                              1);

        if (result == EXIT_FAILURE)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR: ZeroMQ Error publishing events";
            std::cout << std::endl;
        }

        // Cleanup vector
        global_status.waveforms_buffer.clear();
        // Initialize vector size to its max size plus a 10%
        global_status.waveforms_buffer.reserve(global_status.events_buffer_max_size
                                               + global_status.events_buffer_max_size / 10);
    }
}

void actions::generic::publish_message(status &global_status,
                                       std::string topic,
                                       json_t *status_message)
{
    std::chrono::time_point<std::chrono::system_clock> last_publication = std::chrono::system_clock::now();
    global_status.last_publication = last_publication;

    void *status_socket = global_status.status_socket;
    const unsigned long int status_msg_ID = global_status.status_msg_ID;

    char time_buffer[BUFFER_SIZE];
    time_string(time_buffer, BUFFER_SIZE, NULL);

    json_object_set_new(status_message, "module", json_string("abcd"));
    json_object_set_new(status_message, "timestamp", json_string(time_buffer));
    json_object_set_new(status_message, "msg_ID", json_integer(status_msg_ID));

    char *output_buffer = json_dumps(status_message, JSON_COMPACT);

    if (!output_buffer)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to allocate status output buffer; ";
        std::cout << std::endl;
    }
    else
    {
        size_t total_size = strlen(output_buffer);

        topic += "_v0_s";
        topic += std::to_string(total_size);

        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Sending status message; ";
            std::cout << "Topic: " << topic << "; ";
            std::cout << "buffer size: " << total_size << "; ";
            std::cout << "message: " << output_buffer << "; ";
            std::cout << std::endl;
        }

        send_byte_message(status_socket, topic.c_str(), output_buffer, total_size, 1);

        free(output_buffer);
    }

    global_status.status_msg_ID += 1;
}

void actions::generic::stop_acquisition(status &global_status)
{
    const unsigned int verbosity = global_status.verbosity;

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "#### Stopping acquisition!!! ";
        std::cout << std::endl;
    }

    // Stop acquisition
    FDwfAnalogInConfigure(global_status.ad2_handler, 0, false);

    std::chrono::time_point<std::chrono::system_clock> stop_time = std::chrono::system_clock::now();
    auto delta_time = std::chrono::duration_cast<std::chrono::duration<long int>>(stop_time - global_status.start_time);

    global_status.stop_time = stop_time;

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Run time: ";
        std::cout << delta_time.count();
        std::cout << std::endl;
    }
}

void actions::generic::clear_memory(status &global_status)
{
    global_status.counts.clear();
    global_status.partial_counts.clear();
    global_status.waveforms_buffer.clear();
}

void actions::generic::destroy_digitizer(status &global_status)
{
    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Destroying digitizer ";
        std::cout << std::endl;
    }

    FDwfDeviceClose(global_status.ad2_handler);
    global_status.ad2_handler = hdwfNone;
}

bool actions::generic::create_digitizer(status &global_status)
{
    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Opening device number: " << global_status.device_number << "; ";
        std::cout << std::endl;
    }

    const auto result = FDwfDeviceOpen(global_status.device_number,
                                       &global_status.ad2_handler);

    if (!result)
    {
        char szError[512];
        FDwfGetLastErrorMsg(szError);

        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to open device: " << szError;
        std::cout << std::endl;

        return false;
    }

    return true;
}

bool actions::generic::configure_digitizer(status &global_status)
{
    unsigned int verbosity = global_status.verbosity;

    global_status.channels_ranges.clear();
    global_status.channels_offsets.clear();
    global_status.enabled_channels.clear();

    json_t *config = global_status.config;

    const HDWF ad2_handler = global_status.ad2_handler;

    if (ad2_handler == hdwfNone)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Digitizer unavailable";
        std::cout << std::endl;

        return false;
    }

    // Get the number of available analog in channels
    FDwfAnalogInChannelCount(ad2_handler, &global_status.channels_number);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Number of available channels: " << global_status.channels_number << "; ";
        std::cout << std::endl;
    }

    // Reading the sampling rate info
    double sample_min;
    double sample_max;

    FDwfAnalogInFrequencyInfo(ad2_handler, &sample_min, &sample_max);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Sample frequency min: " << sample_min << "; ";
        std::cout << "Sample frequency max: " << sample_max << "; ";
        std::cout << std::endl;
    }

    double sample_rate = json_number_value(json_object_get(config, "sample_rate"));

    if (sample_rate < sample_min || sample_max < sample_rate)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Sampling rate is out of bounds! ";
        std::cout << "Range: [" << sample_min << ", " << sample_max << "] ";
        std::cout << "Got: " << sample_rate << "; ";
        std::cout << std::endl;

        // Setting a default sample_rate value
        FDwfAnalogInFrequencySet(ad2_handler, sample_max);
    }
    else
    {
        FDwfAnalogInFrequencySet(ad2_handler, sample_rate);
    }

    // Reading the set frequency
    double true_sample_rate;

    FDwfAnalogInFrequencyGet(ad2_handler, &true_sample_rate);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "True sample frequency: " << true_sample_rate << "; ";
        std::cout << std::endl;
    }

    // Reading the buffer size info
    int size_min;
    int size_max;

    FDwfAnalogInBufferSizeInfo(ad2_handler, &size_min, &size_max);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Buffer size min: " << size_min << "; ";
        std::cout << "Buffer size max: " << size_max << "; ";
        std::cout << std::endl;
    }

    int buffer_size = json_integer_value(json_object_get(config, "buffer_size"));

    if (buffer_size < size_min || size_max < buffer_size)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Buffer size is out of bounds! ";
        std::cout << "Range: [" << size_min << ", " << size_max << "] ";
        std::cout << "Got: " << buffer_size << "; ";
        std::cout << std::endl;

        // Setting a default buffer size value
        FDwfAnalogInBufferSizeSet(ad2_handler, size_max);

        global_status.ad2_buffer_size = size_max;
    }
    else
    {
        FDwfAnalogInBufferSizeSet(ad2_handler, buffer_size);

        global_status.ad2_buffer_size = buffer_size;
    }

    // Reading the set buffer size
    int true_buffer_size;

    FDwfAnalogInBufferSizeGet(ad2_handler, &true_buffer_size);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "True buffer size: " << true_buffer_size << "; ";
        std::cout << std::endl;
    }

    // Reading the acquisition mode info
    int acquisition_modes;
    FDwfAnalogInAcquisitionModeInfo(ad2_handler, &acquisition_modes);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Available acquisition modes:" << std::endl;
        std::cout << "single: " << IsBitSet(acquisition_modes, acqmodeSingle) << std::endl;
        std::cout << "scan_shift: " << IsBitSet(acquisition_modes, acqmodeScanShift) << std::endl;
        std::cout << "scan_screen: " << IsBitSet(acquisition_modes, acqmodeScanScreen) << std::endl;
        std::cout << "record: " << IsBitSet(acquisition_modes, acqmodeRecord) << std::endl;
        std::cout << "overs: " << IsBitSet(acquisition_modes, acqmodeOvers) << std::endl;
        std::cout << "single1: " << IsBitSet(acquisition_modes, acqmodeSingle1) << std::endl;
    }

    std::string acquisition_mode = json_string_value(json_object_get(config, "acquisition_mode"));

    if (acquisition_mode == "single")
    {
        FDwfAnalogInAcquisitionModeSet(ad2_handler, acqmodeSingle);
    }
    else if (acquisition_mode == "scan_shift")
    {
        FDwfAnalogInAcquisitionModeSet(ad2_handler, acqmodeScanShift);
    }
    else if (acquisition_mode == "scan_screen")
    {
        FDwfAnalogInAcquisitionModeSet(ad2_handler, acqmodeScanScreen);
    }
    else if (acquisition_mode == "record")
    {
        FDwfAnalogInAcquisitionModeSet(ad2_handler, acqmodeRecord);
    }
    else if (acquisition_mode == "overs")
    {
        FDwfAnalogInAcquisitionModeSet(ad2_handler, acqmodeOvers);
    }
    else if (acquisition_mode == "single1")
    {
        FDwfAnalogInAcquisitionModeSet(ad2_handler, acqmodeSingle1);
    }
    else
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unrecognized acquisition mode; ";
        std::cout << "Got: " << acquisition_mode << "; ";
        std::cout << std::endl;

        FDwfAnalogInAcquisitionModeSet(ad2_handler, acqmodeSingle);
    }

    // Reading the channel offset info
    double offset_min;
    double offset_max;
    double offset_steps_number_d;
    FDwfAnalogInChannelOffsetInfo(ad2_handler, &offset_min, &offset_max, &offset_steps_number_d);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "offset min: " << offset_min << "; ";
        std::cout << "offset max: " << offset_max << "; ";
        std::cout << "Steps number: " << offset_steps_number_d << "; ";
        std::cout << std::endl;
    }

    // Reading the channel range info
    double range_min;
    double range_max;
    double range_steps_number_d;
    double range_steps[32];
    int range_steps_number;
    FDwfAnalogInChannelRangeInfo(ad2_handler, &range_min, &range_max, &range_steps_number_d);
    FDwfAnalogInChannelRangeSteps(ad2_handler, range_steps, &range_steps_number);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Range min: " << range_min << "; ";
        std::cout << "Range max: " << range_max << "; ";
        std::cout << "Steps number: " << range_steps_number << ", " << range_steps_number_d << "; ";
        std::cout << std::endl;

        for (int i = 0; i < range_steps_number; i++)
        {
            std::cout << "[" << i << "] step: " << range_steps[i] << "; ";
            std::cout << std::endl;
        }

        std::cout << std::endl;
    }

    json_t *trigger_config = json_object_get(config, "trigger");

    // Reading the trigger sources info
    int trigger_sources;
    FDwfAnalogInTriggerSourceInfo(ad2_handler, &trigger_sources);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Available trigger sources:" << std::endl;
        std::cout << "none: " << IsBitSet(trigger_sources, trigsrcNone) << std::endl;
        std::cout << "PC: " << IsBitSet(trigger_sources, trigsrcPC) << std::endl;
        std::cout << "detector_analog_in: " << IsBitSet(trigger_sources, trigsrcDetectorAnalogIn) << std::endl;
        std::cout << "detector_digital_in: " << IsBitSet(trigger_sources, trigsrcDetectorDigitalIn) << std::endl;
        std::cout << "analog_in: " << IsBitSet(trigger_sources, trigsrcAnalogIn) << std::endl;
        std::cout << "digital_in: " << IsBitSet(trigger_sources, trigsrcDigitalIn) << std::endl;
        std::cout << "digital_out: " << IsBitSet(trigger_sources, trigsrcDigitalOut) << std::endl;
        std::cout << "analog_out1: " << IsBitSet(trigger_sources, trigsrcAnalogOut1) << std::endl;
        std::cout << "analog_out2: " << IsBitSet(trigger_sources, trigsrcAnalogOut2) << std::endl;
        std::cout << "analog_out3: " << IsBitSet(trigger_sources, trigsrcAnalogOut3) << std::endl;
        std::cout << "analog_out4: " << IsBitSet(trigger_sources, trigsrcAnalogOut4) << std::endl;
        std::cout << "external1: " << IsBitSet(trigger_sources, trigsrcExternal1) << std::endl;
        std::cout << "external2: " << IsBitSet(trigger_sources, trigsrcExternal2) << std::endl;
        std::cout << "external3: " << IsBitSet(trigger_sources, trigsrcExternal3) << std::endl;
        std::cout << "external4: " << IsBitSet(trigger_sources, trigsrcExternal4) << std::endl;
        std::cout << "high: " << IsBitSet(trigger_sources, trigsrcHigh) << std::endl;
        std::cout << "low: " << IsBitSet(trigger_sources, trigsrcLow) << std::endl;
    }

    std::string trigger_source = json_string_value(json_object_get(trigger_config, "source"));

    if (trigger_source == "none")
    {
        FDwfAnalogInTriggerSourceSet(ad2_handler, trigsrcNone);
    }
    else if (trigger_source == "PC")
    {
        FDwfAnalogInTriggerSourceSet(ad2_handler, trigsrcPC);
    }
    else if (trigger_source == "detector_analog_in")
    {
        FDwfAnalogInTriggerSourceSet(ad2_handler, trigsrcDetectorAnalogIn);
    }
    else if (trigger_source == "detector_digital_in")
    {
        FDwfAnalogInTriggerSourceSet(ad2_handler, trigsrcDetectorDigitalIn);
    }
    else if (trigger_source == "analog_in")
    {
        FDwfAnalogInTriggerSourceSet(ad2_handler, trigsrcAnalogIn);
    }
    else if (trigger_source == "digital_in")
    {
        FDwfAnalogInTriggerSourceSet(ad2_handler, trigsrcDigitalIn);
    }
    else if (trigger_source == "digital_out")
    {
        FDwfAnalogInTriggerSourceSet(ad2_handler, trigsrcDigitalOut);
    }
    else if (trigger_source == "analog_out1")
    {
        FDwfAnalogInTriggerSourceSet(ad2_handler, trigsrcAnalogOut1);
    }
    else if (trigger_source == "analog_out2")
    {
        FDwfAnalogInTriggerSourceSet(ad2_handler, trigsrcAnalogOut2);
    }
    else if (trigger_source == "analog_out3")
    {
        FDwfAnalogInTriggerSourceSet(ad2_handler, trigsrcAnalogOut3);
    }
    else if (trigger_source == "analog_out4")
    {
        FDwfAnalogInTriggerSourceSet(ad2_handler, trigsrcAnalogOut4);
    }
    else if (trigger_source == "external1")
    {
        FDwfAnalogInTriggerSourceSet(ad2_handler, trigsrcExternal1);
    }
    else if (trigger_source == "external2")
    {
        FDwfAnalogInTriggerSourceSet(ad2_handler, trigsrcExternal2);
    }
    else if (trigger_source == "external3")
    {
        FDwfAnalogInTriggerSourceSet(ad2_handler, trigsrcExternal3);
    }
    else if (trigger_source == "external4")
    {
        FDwfAnalogInTriggerSourceSet(ad2_handler, trigsrcExternal4);
    }
    else if (trigger_source == "high")
    {
        FDwfAnalogInTriggerSourceSet(ad2_handler, trigsrcHigh);
    }
    else if (trigger_source == "low")
    {
        FDwfAnalogInTriggerSourceSet(ad2_handler, trigsrcLow);
    }
    else
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unrecognized trigger source; ";
        std::cout << "Got: " << trigger_source << "; ";
        std::cout << std::endl;
    }

    // Reading the trigger position info
    double trigger_position_min;
    double trigger_position_max;
    double trigger_position_steps;

    FDwfAnalogInTriggerPositionInfo(ad2_handler, &trigger_position_min, &trigger_position_max, &trigger_position_steps);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Trigger position min: " << trigger_position_min << "; ";
        std::cout << "Trigger position max: " << trigger_position_max << "; ";
        std::cout << "Trigger position steps: " << trigger_position_steps << "; ";
        std::cout << std::endl;
    }

    double trigger_position = json_number_value(json_object_get(trigger_config, "position"));

    if (trigger_position < trigger_position_min || trigger_position_max < trigger_position)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Trigger position is out of bounds! ";
        std::cout << "Range: [" << trigger_position_min << ", " << trigger_position_max << "] ";
        std::cout << "Got: " << trigger_position << "; ";
        std::cout << std::endl;

        // Setting a default trigger position value
        FDwfAnalogInTriggerPositionSet(ad2_handler, trigger_position_min);
    }
    else
    {
        FDwfAnalogInTriggerPositionSet(ad2_handler, trigger_position);
    }

    // Reading the set trigger position
    double true_trigger_position;

    FDwfAnalogInTriggerPositionGet(ad2_handler, &true_trigger_position);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "True trigger position: " << true_trigger_position << "; ";
        std::cout << std::endl;
    }

    // Reading the trigger holdoff info
    double trigger_holdoff_min;
    double trigger_holdoff_max;
    double trigger_holdoff_steps;

    FDwfAnalogInTriggerHoldOffInfo(ad2_handler, &trigger_holdoff_min, &trigger_holdoff_max, &trigger_holdoff_steps);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Trigger holdoff min: " << trigger_holdoff_min << "; ";
        std::cout << "Trigger holdoff max: " << trigger_holdoff_max << "; ";
        std::cout << "Trigger holdoff steps: " << trigger_holdoff_steps << "; ";
        std::cout << std::endl;
    }

    double trigger_holdoff = json_number_value(json_object_get(trigger_config, "hold_off"));

    if (trigger_holdoff < trigger_holdoff_min || trigger_holdoff_max < trigger_holdoff)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Trigger holdoff is out of bounds! ";
        std::cout << "Range: [" << trigger_holdoff_min << ", " << trigger_holdoff_max << "] ";
        std::cout << "Got: " << trigger_holdoff << "; ";
        std::cout << std::endl;

        // Setting a default trigger holdoff value
        FDwfAnalogInTriggerHoldOffSet(ad2_handler, trigger_holdoff_min);
    }
    else
    {
        FDwfAnalogInTriggerHoldOffSet(ad2_handler, trigger_holdoff);
    }

    // Reading the set trigger holdoff
    double true_trigger_holdoff;

    FDwfAnalogInTriggerHoldOffGet(ad2_handler, &true_trigger_holdoff);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "True trigger holdoff: " << true_trigger_holdoff << "; ";
        std::cout << std::endl;
    }

    // Reading the trigger channel info
    int trigger_channel_min;
    int trigger_channel_max;

    FDwfAnalogInTriggerChannelInfo(ad2_handler, &trigger_channel_min, &trigger_channel_max);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Trigger channel min: " << trigger_channel_min << "; ";
        std::cout << "Trigger channel max: " << trigger_channel_max << "; ";
        std::cout << std::endl;
    }

    double trigger_channel = json_number_value(json_object_get(trigger_config, "channel"));

    if (trigger_channel < trigger_channel_min || trigger_channel_max < trigger_channel)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Trigger channel is out of bounds! ";
        std::cout << "Range: [" << trigger_channel_min << ", " << trigger_channel_max << "] ";
        std::cout << "Got: " << trigger_channel << "; ";
        std::cout << std::endl;

        // Setting a default trigger channel value
        FDwfAnalogInTriggerChannelSet(ad2_handler, trigger_channel_min);
    }
    else
    {
        FDwfAnalogInTriggerChannelSet(ad2_handler, trigger_channel);
    }

    // Reading the trigger level info
    double trigger_level_min;
    double trigger_level_max;
    double trigger_level_steps;

    FDwfAnalogInTriggerLevelInfo(ad2_handler, &trigger_level_min, &trigger_level_max, &trigger_level_steps);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Trigger level min: " << trigger_level_min << "; ";
        std::cout << "Trigger level max: " << trigger_level_max << "; ";
        std::cout << "Trigger level steps: " << trigger_level_steps << "; ";
        std::cout << std::endl;
    }

    double trigger_level = json_number_value(json_object_get(trigger_config, "level"));

    if (trigger_level < trigger_level_min || trigger_level_max < trigger_level)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Trigger level is out of bounds! ";
        std::cout << "Range: [" << trigger_level_min << ", " << trigger_level_max << "] ";
        std::cout << "Got: " << trigger_level << "; ";
        std::cout << std::endl;

        // Setting a default trigger level value
        FDwfAnalogInTriggerLevelSet(ad2_handler, trigger_level_min);
    }
    else
    {
        FDwfAnalogInTriggerLevelSet(ad2_handler, trigger_level);
    }

    // Reading the set trigger level
    double true_trigger_level;

    FDwfAnalogInTriggerLevelGet(ad2_handler, &true_trigger_level);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "True trigger level: " << true_trigger_level << "; ";
        std::cout << std::endl;
    }

    // Reading the trigger types info
    int trigger_types;
    FDwfAnalogInTriggerTypeInfo(ad2_handler, &trigger_types);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Available trigger types:" << std::endl;
        std::cout << "edge: " << IsBitSet(trigger_types, trigtypeEdge) << std::endl;
        std::cout << "pulse: " << IsBitSet(trigger_types, trigtypePulse) << std::endl;
        std::cout << "transition: " << IsBitSet(trigger_types, trigtypeTransition) << std::endl;
    }

    std::string trigger_type = json_string_value(json_object_get(trigger_config, "type"));

    if (trigger_type == "edge")
    {
        FDwfAnalogInTriggerTypeSet(ad2_handler, trigtypeEdge);
    }
    else if (trigger_type == "pulse")
    {
        FDwfAnalogInTriggerTypeSet(ad2_handler, trigtypePulse);
    }
    else if (trigger_type == "transition")
    {
        FDwfAnalogInTriggerTypeSet(ad2_handler, trigtypeTransition);
    }
    else
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unrecognized trigger type; ";
        std::cout << "Got: " << trigger_type << "; ";
        std::cout << std::endl;

        FDwfAnalogInTriggerTypeSet(ad2_handler, trigtypeEdge);
    }

    // Reading the trigger condition info
    int trigger_conditions;
    FDwfAnalogInTriggerConditionInfo(ad2_handler, &trigger_conditions);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Available trigger conditions:" << std::endl;
        std::cout << "rising_positive: " << IsBitSet(trigger_conditions, trigcondRisingPositive) << std::endl;
        std::cout << "falling_negative: " << IsBitSet(trigger_conditions, trigcondFallingNegative) << std::endl;
    }

    std::string trigger_condition = json_string_value(json_object_get(trigger_config, "condition"));

    if (trigger_condition == "rising_positive")
    {
        FDwfAnalogInTriggerConditionSet(ad2_handler, trigcondRisingPositive);
    }
    else if (trigger_condition == "falling_negative")
    {
        FDwfAnalogInTriggerConditionSet(ad2_handler, trigcondFallingNegative);
    }
    else
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unrecognized trigger condition; ";
        std::cout << "Got: " << trigger_condition << "; ";
        std::cout << std::endl;

        FDwfAnalogInTriggerConditionSet(ad2_handler, trigcondFallingNegative);
    }

    // Starting the single channels configuration
    json_t *json_channels = json_object_get(config, "channels");

    if (json_channels != NULL && json_is_array(json_channels))
    {
        size_t index;
        json_t *value;

        json_array_foreach(json_channels, index, value) {
            json_t *json_id = json_object_get(value, "id");

            if (json_id != NULL && json_is_integer(json_id)) {
                const int id = json_integer_value(json_id);

                if (verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Found channel: " << id << "; ";
                    std::cout << std::endl;
                }

                const bool enabled = json_is_true(json_object_get(value, "enable"));

                if (verbosity > 0 && enabled)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Channel is enabled; ";
                    std::cout << std::endl;
                }
                else if (verbosity > 0 && !enabled)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Channel is disabled; ";
                    std::cout << std::endl;
                }

                if (enabled)
                {
                    global_status.enabled_channels.push_back(id);
                }

                FDwfAnalogInChannelEnableSet(ad2_handler, id, enabled);

                double range = json_number_value(json_object_get(value, "range"));

                if (range < range_min || range_max < range)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Range is out of bounds! ";
                    std::cout << "Range: [" << range_min << ", " << range_max << "] ";
                    std::cout << "Got: " << range << "; ";
                    std::cout << std::endl;

                    // Setting a default range value
                    FDwfAnalogInChannelRangeSet(ad2_handler, id, range_min);
                }
                else
                {
                    for (size_t i = (range_steps_number > 0 ? range_steps_number : 0); i > 0; i--)
                    {
                        // Looking for the first step value that is smaller than the range
                        // we use i-1 because we start from range_steps_number and we end at 1.
                        const double this_step = range_steps[i - 1];

                        if (range >= this_step)
                        {
                            if (verbosity > 0)
                            {
                                char time_buffer[BUFFER_SIZE];
                                time_string(time_buffer, BUFFER_SIZE, NULL);
                                std::cout << '[' << time_buffer << "] ";
                                std::cout << "Using range: " << this_step << "; ";
                                std::cout << std::endl;
                            }
    
                            FDwfAnalogInChannelRangeSet(ad2_handler, id, this_step);
                        }
                    }
                }

                // Reading the set trigger level
                double true_channel_range;

                FDwfAnalogInChannelRangeGet(ad2_handler, id, &true_channel_range);

                if (verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "True channel range: " << true_channel_range << "; ";
                    std::cout << std::endl;
                }

                global_status.channels_ranges[id] = true_channel_range;

                double offset = json_number_value(json_object_get(value, "offset"));

                if (offset < offset_min || offset_max < offset)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Offset is out of bounds! ";
                    std::cout << "Range: [" << offset_min << ", " << offset_max << "] ";
                    std::cout << "Got: " << offset << "; ";
                    std::cout << std::endl;

                    const double default_offset = (offset_max + offset_min) / 2;

                    // Setting a default offset value
                    FDwfAnalogInChannelOffsetSet(ad2_handler, id, default_offset);
                }
                else
                {
                    FDwfAnalogInChannelOffsetSet(ad2_handler, id, offset);
                }

                // Reading the set channel offset
                double true_channel_offset;

                FDwfAnalogInChannelOffsetGet(ad2_handler, id, &true_channel_offset);

                if (verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "True channel offset: " << true_channel_offset << "; ";
                    std::cout << std::endl;
                }

                global_status.channels_offsets[id] = true_channel_offset;
            }
        }
    }
    
    return true;
}

bool actions::generic::allocate_memory(status &global_status)
{
    unsigned int verbosity = global_status.verbosity;

    global_status.counts.clear();
    global_status.counts.resize(global_status.channels_number, 0);
    global_status.partial_counts.clear();
    global_status.partial_counts.resize(global_status.channels_number, 0);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Memory allocation";
        std::cout << std::endl;
    }

    // Reserve the events_buffer in order to have a good starting size of its buffer
    global_status.waveforms_buffer.reserve(global_status.events_buffer_max_size
                                           + global_status.events_buffer_max_size / 10);

    return true;
}

/******************************************************************************/
/* Specific actions                                                           */
/******************************************************************************/

state actions::start(status&)
{
    return states::CREATE_CONTEXT;
}

state actions::create_context(status &global_status)
{
    // Creates a ØMQ context
    void *context = zmq_ctx_new();
    if (!context)
    {
        // No errors are defined for this function
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on context creation";
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    global_status.context = context;

    std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));

    return states::CREATE_SOCKETS;
}

state actions::create_sockets(status &global_status)
{
    void *context = global_status.context;

    // Creates the status socket
    void *status_socket = zmq_socket(context, ZMQ_PUB);
    if (!status_socket)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on status socket creation: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Creates the data socket
    void *data_socket = zmq_socket(context, ZMQ_PUB);
    if (!data_socket)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on data socket creation: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Creates the commands socket
    void *commands_socket = zmq_socket(context, ZMQ_PULL);
    if (!commands_socket)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on commands socket creation: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    global_status.status_socket = status_socket;
    global_status.data_socket = data_socket;
    global_status.commands_socket = commands_socket;

    std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));

    return states::BIND_SOCKETS;
}

state actions::bind_sockets(status &global_status)
{
    std::string status_address = global_status.status_address;
    std::string data_address = global_status.data_address;
    std::string commands_address = global_status.commands_address;

    // Binds the status socket to its address
    const int s = zmq_bind(global_status.status_socket, status_address.c_str());
    if (s != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on status socket binding: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Binds the data socket to its address
    const int d = zmq_bind(global_status.data_socket, data_address.c_str());
    if (d != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on data socket binding: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Binds the socket to its address
    const int c = zmq_bind(global_status.commands_socket, commands_address.c_str());
    if (c != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on commands socket binding: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));

    return states::READ_CONFIG;
}

state actions::read_config(status &global_status)
{
    const std::string config_file_name = global_status.config_file;

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Reading config file: " << config_file_name << " ";
        std::cout << std::endl;
    }

    json_error_t error;

    json_t *new_config = json_load_file(config_file_name.c_str(), 0, &error);

    if (!new_config)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Parse error while reading config file: ";
        std::cout << error.text;
        std::cout << " (source: ";
        std::cout << error.source;
        std::cout << ", line: ";
        std::cout << error.line;
        std::cout << ", column: ";
        std::cout << error.column;
        std::cout << ", position: ";
        std::cout << error.position;
        std::cout << "); ";
        std::cout << std::endl;

        return states::PARSE_ERROR;
    }
    else
    {
        global_status.config = new_config;

        return states::CREATE_DIGITIZER;
    }
}

state actions::create_digitizer(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string("Digitizer initialization"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    const bool success = actions::generic::create_digitizer(global_status);

    if (success)
    {
        return states::CONFIGURE_DIGITIZER;
    }
    else
    {
        return states::CONFIGURE_ERROR;
    }
}

state actions::recreate_digitizer(status &global_status)
{
    const bool success = actions::generic::create_digitizer(global_status);

    if (success)
    {
        return states::CONFIGURE_DIGITIZER;
    }
    else
    {
        return states::CONFIGURE_ERROR;
    }
}

state actions::configure_digitizer(status &global_status)
{
    const bool success = actions::generic::configure_digitizer(global_status);

    if (success)
    {
        return states::ALLOCATE_MEMORY;
    }
    else
    {
        return states::CONFIGURE_ERROR;
    }
}

state actions::reconfigure_destroy_digitizer(status &global_status)
{
    actions::generic::destroy_digitizer(global_status);

    return states::RECREATE_DIGITIZER;
}

state actions::receive_commands(status &global_status)
{
    void *commands_socket = global_status.commands_socket;

    char *buffer;
    size_t size;

    const int result = receive_byte_message(commands_socket, nullptr, (void**)(&buffer), &size, 0, global_status.verbosity);

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "result: " << result << "; ";
        std::cout << "size: " << size << "; ";
        std::cout << std::endl;
    }

    if (size > 0 && result == EXIT_SUCCESS)
    {
        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Message buffer: ";
            std::cout << (char*)buffer;
            std::cout << "; ";
            std::cout << std::endl;
        }

        std::string error_string;

        json_error_t error;
        json_t *json_message = json_loadb(buffer, size, 0, &error);

        free(buffer);

        if (!json_message)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR: ";
            std::cout << error.text;
            std::cout << " (source: ";
            std::cout << error.source;
            std::cout << ", line: ";
            std::cout << error.line;
            std::cout << ", column: ";
            std::cout << error.column;
            std::cout << ", position: ";
            std::cout << error.position;
            std::cout << "); ";
            std::cout << std::endl;
        }
        else
        {
            const size_t command_ID = json_integer_value(json_object_get(json_message, "msg_ID"));
            const std::string command = json_string_value(json_object_get(json_message, "command"));
            json_t *json_arguments = json_object_get(json_message, "arguments");

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Command ID: " << command_ID << "; ";
                std::cout << std::endl;
            }

            if (command == std::string("start"))
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "################################################################### Start!!! ###";
                std::cout << std::endl;

                return states::START_ACQUISITION;
            }
            else if (command == std::string("reconfigure") && json_arguments)
            {
                json_t *new_config = json_object_get(json_arguments, "config");

                // Remember to clean up
                json_decref(global_status.config);

                // This is a borrowed reference, so we shall increment the reference count
                global_status.config = new_config;
                json_incref(global_status.config);

                json_t *json_event_message = json_object();

                json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
                json_object_set_new_nocheck(json_event_message, "event", json_string("Digitizer reconfiguration"));

                actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

                json_decref(json_event_message);

                return states::CONFIGURE_DIGITIZER;
            }
            else if (command == std::string("off"))
            {
                return states::CLEAR_MEMORY;
            }
            else if (command == std::string("quit"))
            {
                return states::CLEAR_MEMORY;
            }

            json_decref(json_message);
        }
    }

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    if (now - global_status.last_publication > std::chrono::seconds(defaults_abcd_publish_timeout))
    {
        return states::PUBLISH_STATUS;
    }

    return states::RECEIVE_COMMANDS;
}

state actions::publish_status(status &global_status)
{
    DwfState ad2_status;
    const bool poll_success = FDwfAnalogInStatus(global_status.ad2_handler, false, &ad2_status);

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";

        if (ad2_status == DwfStateReady)
        {
            std::cout << "Analog in status: Ready; ";
        }
        else if (ad2_status == DwfStateConfig)
        {
            std::cout << "Analog in status: Config; ";
        }
        else if (ad2_status == DwfStatePrefill)
        {
            std::cout << "Analog in status: Prefill; ";
        }
        else if (ad2_status == DwfStateArmed)
        {
            std::cout << "Analog in status: Armed; ";
        }
        else if (ad2_status == DwfStateWait)
        {
            std::cout << "Analog in status: Wait; ";
        }
        else if (ad2_status == DwfStateTriggered)
        {
            std::cout << "Analog in status: Triggered; ";
        }
        else if (ad2_status == DwfStateRunning)
        {
            std::cout << "Analog in status: Running; ";
        }
        else if (ad2_status == DwfStateDone)
        {
            std::cout << "Analog in status: Done; ";
        }
        else
        {
            std::cout << "Analog in status: " << static_cast<int>(ad2_status) << "; ";
        }
        std::cout << "Poll success: " << poll_success << "; ";
        std::cout << std::endl;
    }

    json_t *status_message = json_object();

    json_object_set_new_nocheck(status_message, "config", json_deep_copy(global_status.config));

    json_t *acquisition = json_object();
    json_object_set_new_nocheck(acquisition, "running", json_false());
    json_object_set_new_nocheck(status_message, "acquisition", acquisition);

    json_t *digitizer = json_object();

    if (global_status.ad2_handler == hdwfNone || poll_success == false)
    {
        json_object_set_new_nocheck(digitizer, "valid_pointer", json_false());
        json_object_set_new_nocheck(digitizer, "active", json_false());
    }
    else
    {
        json_object_set_new_nocheck(digitizer, "valid_pointer", json_true());
        json_object_set_new_nocheck(digitizer, "active", json_true());
    }

    json_object_set_new_nocheck(status_message, "digitizer", digitizer);

    actions::generic::publish_message(global_status, defaults_abcd_status_topic, status_message);

    json_decref(status_message);

    if (global_status.ad2_handler != hdwfNone && poll_success == true)
    {
        return states::RECEIVE_COMMANDS;
    }
    else
    {
        return states::DIGITIZER_ERROR;
    }
}

state actions::allocate_memory(status &global_status)
{
    const bool success = actions::generic::allocate_memory(global_status);

    if (success)
    {
        return states::PUBLISH_STATUS;
    }
    else
    {
        return states::DIGITIZER_ERROR;
    }
}

state actions::restart_allocate_memory(status &global_status)
{
    const bool success = actions::generic::allocate_memory(global_status);

    if (success)
    {
        return states::START_ACQUISITION;
    }
    else
    {
        return states::RESTART_CONFIGURE_ERROR;
    }
}

state actions::stop(status&)
{
    return states::STOP;
}

state actions::start_acquisition(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string("Start acquisition"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    std::chrono::time_point<std::chrono::system_clock> start_time = std::chrono::system_clock::now();

    if (global_status.ad2_handler == hdwfNone)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Digitizer pointer unavailable";
        std::cout << std::endl;

        return states::ACQUISITION_ERROR;
    }

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Clearing counters; ";
        std::cout << std::endl;
    }

    const int channels_number = global_status.channels_number;

    global_status.counts.clear();
    global_status.counts.resize(channels_number, 0);
    global_status.partial_counts.clear();
    global_status.partial_counts.resize(channels_number, 0);

    // Start acquisition
    FDwfAnalogInConfigure(global_status.ad2_handler, 0, true);

    global_status.start_time = start_time;

    return states::ACQUISITION_RECEIVE_COMMANDS;
}

state actions::acquisition_receive_commands(status &global_status)
{
    void *commands_socket = global_status.commands_socket;

    char *buffer;
    size_t size;

    const int result = receive_byte_message(commands_socket, nullptr, (void**)(&buffer), &size, 0, global_status.verbosity);

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "result: " << result << "; ";
        std::cout << "size: " << size << "; ";
        std::cout << std::endl;
    }

    if (size > 0 && result == EXIT_SUCCESS)
    {
        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Message buffer: ";
            std::cout << (char*)buffer;
            std::cout << "; ";
            std::cout << std::endl;
        }

        std::string error_string;

        json_error_t error;
        json_t *json_message = json_loadb(buffer, size, 0, &error);

        free(buffer);

        if (!json_message)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR: ";
            std::cout << error.text;
            std::cout << " (source: ";
            std::cout << error.source;
            std::cout << ", line: ";
            std::cout << error.line;
            std::cout << ", column: ";
            std::cout << error.column;
            std::cout << ", position: ";
            std::cout << error.position;
            std::cout << "); ";
            std::cout << std::endl;
        }
        else
        {
            const size_t command_ID = json_integer_value(json_object_get(json_message, "msg_ID"));
            const std::string command = json_string_value(json_object_get(json_message, "command"));

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Command ID: " << command_ID << "; ";
                std::cout << std::endl;
            }

            if (command == std::string("stop"))
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "#################################################################### Stop!!! ###";
                std::cout << std::endl;

                return states::STOP_PUBLISH_EVENTS;
            }

            json_decref(json_message);
        }
    }

    return states::ADD_TO_BUFFER;
}

state actions::add_to_buffer(status &global_status)
{
    const unsigned int verbosity = global_status.verbosity;
    const HDWF ad2_handler = global_status.ad2_handler;

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();

    if (ad2_handler == hdwfNone)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Digitizer unavailable";
        std::cout << std::endl;
        return states::ACQUISITION_ERROR;
    }

    DwfState ad2_status;

    const bool poll_success = FDwfAnalogInStatus(ad2_handler, true, &ad2_status);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        if (ad2_status == DwfStateReady)
        {
            std::cout << "Analog in status: Ready; ";
        }
        else if (ad2_status == DwfStateConfig)
        {
            std::cout << "Analog in status: Config; ";
        }
        else if (ad2_status == DwfStatePrefill)
        {
            std::cout << "Analog in status: Prefill; ";
        }
        else if (ad2_status == DwfStateArmed)
        {
            std::cout << "Analog in status: Armed; ";
        }
        else if (ad2_status == DwfStateWait)
        {
            std::cout << "Analog in status: Wait; ";
        }
        else if (ad2_status == DwfStateTriggered)
        {
            std::cout << "Analog in status: Triggered; ";
        }
        else if (ad2_status == DwfStateRunning)
        {
            std::cout << "Analog in status: Running; ";
        }
        else if (ad2_status == DwfStateDone)
        {
            std::cout << "Analog in status: Done; ";
        }
        else
        {
            std::cout << "Analog in status: " << static_cast<int>(ad2_status) << "; ";
        }
        std::cout << "Poll success: " << poll_success << "; ";
        std::cout << std::endl;
    }

    if (verbosity > 0)
    {
        const std::chrono::time_point<std::chrono::system_clock> after_poll = std::chrono::system_clock::now();
        const auto poll_duration = std::chrono::duration_cast<nanoseconds>(after_poll - now).count();

        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Poll duration: " << poll_duration * 1e-6 << " ms; ";
        std::cout << std::endl;
    }

    if (!poll_success)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Digitizer error";
        std::cout << std::endl;

        return states::ACQUISITION_ERROR;
    }

    //int samples_left;
    //FDwfAnalogInStatusSamplesLeft(ad2_handler, &samples_left);

    //if (verbosity > 0)
    //{
    //    char time_buffer[BUFFER_SIZE];
    //    time_string(time_buffer, BUFFER_SIZE, NULL);
    //    std::cout << '[' << time_buffer << "] ";
    //    std::cout << "Samples left: " << samples_left << "; ";
    //    std::cout << std::endl;
    //}

    //int samples_valid;
    //FDwfAnalogInStatusSamplesLeft(ad2_handler, &samples_valid);

    //if (verbosity > 0)
    //{
    //    char time_buffer[BUFFER_SIZE];
    //    time_string(time_buffer, BUFFER_SIZE, NULL);
    //    std::cout << '[' << time_buffer << "] ";
    //    std::cout << "Samples valid: " << samples_valid << "; ";
    //    std::cout << std::endl;
    //}

    //int data_available;
    //int data_lost;
    //int data_corrupt;
    //FDwfAnalogInStatusRecord(ad2_handler, &data_available, &data_lost, &data_corrupt);

    //if (verbosity > 0)
    //{
    //    char time_buffer[BUFFER_SIZE];
    //    time_string(time_buffer, BUFFER_SIZE, NULL);
    //    std::cout << '[' << time_buffer << "] ";
    //    std::cout << "Data available: " << data_available << " (" << data_available / global_status.ad2_buffer_size << " packets, " << data_available % global_status.ad2_buffer_size << "); ";
    //    std::cout << "Data lost: " << data_lost << "; ";
    //    std::cout << "Data corrupt: " << data_corrupt << "; ";
    //    std::cout << std::endl;
    //}

    if (ad2_status == DwfStateDone)
    {
        if (verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Downloading samples; ";
            std::cout << std::endl;
        }

        //std::vector<double> double_samples(global_status.ad2_buffer_size);
        std::vector<int16_t> int_samples(global_status.ad2_buffer_size);

        // We have events...
        for (auto &channel: global_status.enabled_channels)
        {
            // We fake a timestamp by reading the computer clock at the moment of the request.
            const auto Delta = std::chrono::duration_cast<nanoseconds>(now - global_status.start_time);
            // We divide by 10 so we can have the same granularity as the samples in the waveform
            // because the minimum sample spacing is 10 ns.
            const uint64_t timestamp = Delta.count() / 10;

            if (verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Downloading samples from channel: " << channel << "; ";
                std::cout << std::endl;
            }

            const std::chrono::time_point<std::chrono::system_clock> before_data_reading = std::chrono::system_clock::now();

            // According to attila in the Digilent's forum:
            // The cdData specifies the number of samples to return and the
            // idxData specifies the start index.
            // The only situation when you need idxData to be other than zero
            // would be with recording capture, when the newly captured sample
            // chunk would overflow the recording buffer. In this case you can
            // get with one call the N samples which fit in your buffer and
            // with a second call remaining samples starting with index N. 
            const bool download_success = FDwfAnalogInStatusData16(ad2_handler,
                                                                   channel,
                                                                   int_samples.data(),
                                                                   0,
                                                                   global_status.ad2_buffer_size);
            //const bool download_success = FDwfAnalogInStatusData(ad2_handler,
            //                                                     channel,
            //                                                     double_samples.data(),
            //                                                     global_status.ad2_buffer_size);

            if (verbosity > 0)
            {
                const auto data_reading_duration = std::chrono::duration_cast<nanoseconds>(std::chrono::system_clock::now() - before_data_reading).count();

                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Data reading duration: " << data_reading_duration * 1e-6 << " ms; ";
                std::cout << std::endl;
            }

            if (!download_success)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "ERROR: Unable to read data";
                std::cout << std::endl;

                return states::ACQUISITION_ERROR;
            }

            // Converting data from double to int
            //const double conversion = 65536.0 / global_status.channels_ranges[channel];
            //const double offset = global_status.channels_offsets[channel];

            //for (int j = 0; j < global_status.ad2_buffer_size; j++)
            //{
            //    int_samples[j] = std::round((double_samples[j] - offset) * conversion);
            //}


            global_status.counts[channel] += 1;
            global_status.partial_counts[channel] += 1;

            global_status.waveforms_buffer.emplace_back(timestamp,
                                                    channel,
                                                    global_status.ad2_buffer_size);

            //memcpy(global_status.waveforms_buffer.back().samples.data(),
            //       int_samples.data(),
            //       global_status.ad2_buffer_size * sizeof(uint16_t));

            // Trasforming the signed int to an unsigned int for compatibility with the rest of ABCD
            for (int j = 0; j < global_status.ad2_buffer_size; j++)
            {
                global_status.waveforms_buffer.back().samples[j] = int_samples[j] + (1 << 15);
            }
        }

        if (verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Waveforms buffer size: " << global_status.waveforms_buffer.size() << "; ";
            std::cout << std::endl;
        }

        if (global_status.waveforms_buffer.size() >= global_status.events_buffer_max_size)
        {
            return states::PUBLISH_EVENTS;
        }
    }

    if (now - global_status.last_publication > std::chrono::seconds(defaults_abcd_publish_timeout))
    {
        return states::PUBLISH_EVENTS;
    }

    return states::ADD_TO_BUFFER;
}

state actions::publish_events(status &global_status)
{
    actions::generic::publish_events(global_status);

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    if (now - global_status.last_publication > std::chrono::seconds(defaults_abcd_publish_timeout))
    {
        return states::ACQUISITION_PUBLISH_STATUS;
    }

    return states::ADD_TO_BUFFER;
}

state actions::stop_publish_events(status &global_status)
{
    actions::generic::publish_events(global_status);

    return states::STOP_ACQUISITION;
}

state actions::acquisition_publish_status(status &global_status)
{
    DwfState ad2_status;
    const bool poll_success = FDwfAnalogInStatus(global_status.ad2_handler, false, &ad2_status);

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        if (ad2_status == DwfStateReady)
        {
            std::cout << "Analog in status: Ready; ";
        }
        else if (ad2_status == DwfStateConfig)
        {
            std::cout << "Analog in status: Config; ";
        }
        else if (ad2_status == DwfStatePrefill)
        {
            std::cout << "Analog in status: Prefill; ";
        }
        else if (ad2_status == DwfStateArmed)
        {
            std::cout << "Analog in status: Armed; ";
        }
        else if (ad2_status == DwfStateWait)
        {
            std::cout << "Analog in status: Wait; ";
        }
        else if (ad2_status == DwfStateTriggered)
        {
            std::cout << "Analog in status: Triggered; ";
        }
        else if (ad2_status == DwfStateRunning)
        {
            std::cout << "Analog in status: Running; ";
        }
        else if (ad2_status == DwfStateDone)
        {
            std::cout << "Analog in status: Done; ";
        }
        else
        {
            std::cout << "Analog in status: " << static_cast<int>(ad2_status) << "; ";
        }
        std::cout << "Poll success: " << poll_success << "; ";
        std::cout << std::endl;
    }

    json_t *status_message = json_object();

    json_object_set_new_nocheck(status_message, "config", json_deep_copy(global_status.config));

    json_t *acquisition = json_object();
    json_t *digitizer = json_object();

    if (global_status.ad2_handler == hdwfNone || !poll_success)
    {
        json_object_set_new_nocheck(digitizer, "valid_pointer", json_false());
        json_object_set_new_nocheck(digitizer, "active", json_false());
    }
    else
    {
        json_object_set_new_nocheck(digitizer, "valid_pointer", json_true());
        json_object_set_new_nocheck(digitizer, "active", json_true());

        json_object_set_new_nocheck(acquisition, "running", json_true());

        const auto now = std::chrono::system_clock::now();

        const auto run_delta_time = std::chrono::duration_cast<std::chrono::duration<long int>>(now - global_status.start_time);
        const long int runtime = run_delta_time.count();

        json_object_set_new_nocheck(acquisition, "runtime", json_integer(runtime));

        const auto pub_delta_time = std::chrono::duration_cast<std::chrono::duration<long int>>(now - global_status.last_publication);
        const double pubtime = static_cast<double>(pub_delta_time.count());

        json_t *rates = json_array();
        json_t *ICR_rates = json_array();

        json_t *counts = json_array();
        json_t *ICR_counts = json_array();

        if (pubtime > 0)
        {
            for (unsigned int i = 0; i < global_status.partial_counts.size(); i++)
            {
                const double rate = global_status.partial_counts[i] / pubtime;
                json_array_append_new(rates, json_real(rate));
                json_array_append_new(ICR_rates, json_real(rate));
            }
        }
        else
        {
            for (unsigned int i = 0; i < global_status.partial_counts.size(); i++)
            {
                json_array_append_new(rates, json_integer(0));
                json_array_append_new(ICR_rates, json_integer(0));
            }
        }

        for (unsigned int i = 0; i < global_status.counts.size(); i++)
        {
            const unsigned int channel_counts = global_status.counts[i];
            json_array_append_new(counts, json_integer(channel_counts));
            json_array_append_new(ICR_counts, json_integer(channel_counts));
        }

        json_object_set_new_nocheck(acquisition, "rates", rates);
        json_object_set_new_nocheck(acquisition, "ICR_rates", ICR_rates);

        json_object_set_new_nocheck(acquisition, "counts", counts);
        json_object_set_new_nocheck(acquisition, "ICR_counts", ICR_counts);
    }

    json_object_set_new_nocheck(status_message, "acquisition", acquisition);
    json_object_set_new_nocheck(status_message, "digitizer", digitizer);

    actions::generic::publish_message(global_status, defaults_abcd_status_topic, status_message);

    json_decref(status_message);

    // Clear event partial counts
    for (unsigned int i = 0; i < global_status.partial_counts.size(); i++)
    {
        global_status.partial_counts[i] = 0;
    }

    if (global_status.ad2_handler != hdwfNone && poll_success == true)
    {
        return states::ACQUISITION_RECEIVE_COMMANDS;
    }
    else
    {
        return states::ACQUISITION_ERROR;
    }
}

state actions::stop_acquisition(status &global_status)
{
    actions::generic::stop_acquisition(global_status);

    auto delta_time = std::chrono::duration_cast<std::chrono::duration<long int>>(global_status.stop_time - global_status.start_time);
    const std::string event_message = "Stop acquisition (duration: " + std::to_string(delta_time.count()) + " s)";

    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string(event_message.c_str()));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::RECEIVE_COMMANDS;
}

state actions::clear_memory(status &global_status)
{
    if (global_status.ad2_handler != hdwfNone)
    {
        actions::generic::clear_memory(global_status);
    }
    else
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Digitizer pointer unavailable";
        std::cout << std::endl;
    }

    return states::DESTROY_DIGITIZER;
}

state actions::reconfigure_clear_memory(status &global_status)
{
    if (global_status.ad2_handler != hdwfNone)
    {
        actions::generic::clear_memory(global_status);
    }
    else
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Digitizer pointer unavailable";
        std::cout << std::endl;

        return states::CONFIGURE_ERROR;
    }

    return states::RECONFIGURE_DESTROY_DIGITIZER;
}

state actions::destroy_digitizer(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string("Digitizer deactivation"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    actions::generic::destroy_digitizer(global_status);

    return states::CLOSE_SOCKETS;
}

state actions::restart_publish_events(status &global_status)
{
    actions::generic::publish_events(global_status);

    return states::RESTART_STOP_ACQUISITION;
}

state actions::restart_stop_acquisition(status &global_status)
{
    actions::generic::stop_acquisition(global_status);

    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string("Restart stop acquisition"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::RESTART_CLEAR_MEMORY;
}

state actions::restart_clear_memory(status &global_status)
{
    if (global_status.ad2_handler != hdwfNone)
    {
        actions::generic::clear_memory(global_status);
    }
    else
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Digitizer pointer unavailable";
        std::cout << std::endl;
    }

    return states::RESTART_DESTROY_DIGITIZER;
}

state actions::restart_destroy_digitizer(status &global_status)
{
    actions::generic::destroy_digitizer(global_status);

    return states::RESTART_CREATE_DIGITIZER;
}

state actions::restart_create_digitizer(status &global_status)
{
    const bool success = actions::generic::create_digitizer(global_status);

    if (success)
    {
        return states::RESTART_CONFIGURE_DIGITIZER;
    }
    else
    {
        return states::RESTART_CONFIGURE_ERROR;
    }
}

state actions::restart_configure_digitizer(status &global_status)
{
    const bool success = actions::generic::configure_digitizer(global_status);

    if (success)
    {
        return states::RESTART_ALLOCATE_MEMORY;
    }
    else
    {
        return states::RESTART_CONFIGURE_ERROR;
    }
}

state actions::close_sockets(status &global_status)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));

    void *status_socket = global_status.status_socket;
    void *data_socket = global_status.data_socket;
    void *commands_socket = global_status.commands_socket;

    const int s = zmq_close(status_socket);
    if (s != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on status socket close: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    const int d = zmq_close(data_socket);
    if (d != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on data socket close: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    const int c = zmq_close(commands_socket);
    if (c != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on commands socket close: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    return states::DESTROY_CONTEXT;
}

state actions::destroy_context(status &global_status)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));

    void *context = global_status.context;

    const int c = zmq_ctx_destroy(context);
    if (c != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on context destroy: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    return states::STOP;
}

state actions::communication_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Communication error"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::CLOSE_SOCKETS;
}

state actions::parse_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Config parse error"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::CLOSE_SOCKETS;
}

state actions::configure_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Configure error"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::RECONFIGURE_DESTROY_DIGITIZER;
}

state actions::digitizer_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Digitizer error"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::RECONFIGURE_CLEAR_MEMORY;
}

state actions::acquisition_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Acquisition error"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::RESTART_PUBLISH_EVENTS;
}

state actions::restart_configure_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Restart configure error"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::RESTART_DESTROY_DIGITIZER;
}
