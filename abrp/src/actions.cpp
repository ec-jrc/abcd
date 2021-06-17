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

// For system()
#include <stdlib.h>

#include <jansson.h>

extern "C" {
#include "redpitaya/rp.h"

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
    const int stp_result = rp_AcqStop();

    if (stp_result != RP_OK) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to stop acquisition: " << rp_GetError(stp_result) << "; ";
        std::cout << std::endl;
    }

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

    const int rls_result = rp_Release();

    if (rls_result != RP_OK) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to release device: " << rp_GetError(rls_result) << "; ";
        std::cout << std::endl;
    }
}

bool actions::generic::create_digitizer(status &global_status)
{
    // Nope!!! This is a bad idea to do it automatically...
    //
    //if (global_status.verbosity > 0)
    //{
    //    char time_buffer[BUFFER_SIZE];
    //    time_string(time_buffer, BUFFER_SIZE, NULL);
    //    std::cout << '[' << time_buffer << "] ";
    //    std::cout << "Enabling the classic bitstream; ";
    //    std::cout << std::endl;
    //}

    //// This is to solve the "Bus error" issue at the init,
    //// see: https://forum.redpitaya.com/viewtopic.php?t=2081
    //system("cat /opt/redpitaya/fpga/fpga_classic.bit > /dev/xdevcfg");

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Initializing digitizer; ";
        std::cout << std::endl;
    }

    const int result = rp_Init();

    if (result != RP_OK) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to initialize device: " << rp_GetError(result) << "; ";
        std::cout << std::endl;

        return false;
    }

    return true;
}

bool actions::generic::configure_digitizer(status &global_status)
{
    unsigned int verbosity = global_status.verbosity;

    // This is the only option so far
    global_status.channels_number = 2;

    const int rst_result = rp_AcqReset();

    if (rst_result != RP_OK) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to reset acquisition: " << rp_GetError(rst_result) << "; ";
        std::cout << std::endl;

        return false;
    }

    if (global_status.verbosity > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Configuring digitizer; ";
        std::cout << std::endl;
    }

    global_status.enabled_channels.clear();

    ////////////////////////////////////////////////////////////////////////////
    // Starting the global configuration                                      //
    ////////////////////////////////////////////////////////////////////////////
    json_t *config = global_status.config;

    int32_t samples_number = json_integer_value(json_object_get(config, "scope_samples"));

    if (samples_number <= 0 || samples_number > ADC_BUFFER_SIZE) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Invalid scope samples; ";
        std::cout << "Got: " << samples_number << "; ";
        std::cout << std::endl;

        global_status.samples_number = defaults_abrp_scope_samples;
    } else {
        global_status.samples_number = samples_number;
    }

    if (global_status.verbosity > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Scope samples: " << static_cast<unsigned int>(global_status.samples_number) << "; ";
        std::cout << std::endl;
    }

    // Decimation sets the sampling frequency
    int decimation = json_integer_value(json_object_get(config, "decimation"));

    global_status.decimation = decimation;

    rp_acq_decimation_t parsed_decimation = RP_DEC_1;

    if (decimation == 1) {
        parsed_decimation = RP_DEC_1;
    } else if (decimation == 8) {
        parsed_decimation = RP_DEC_8;
    } else if (decimation == 64) {
        parsed_decimation = RP_DEC_64;
    } else if (decimation == 1024) {
        parsed_decimation = RP_DEC_1024;
    } else if (decimation == 8192) {
        parsed_decimation = RP_DEC_8192;
    } else if (decimation == 65536) {
        parsed_decimation = RP_DEC_65536;
    } else {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Invalid decimation; ";
        std::cout << "Got: " << decimation << "; ";
        std::cout << "Possible values: 1, 8, 64, 1024, 8192, 65536; ";
        std::cout << std::endl;

        global_status.decimation = 1;
    }

    const int dcm_result = rp_AcqSetDecimation(parsed_decimation);

    if (dcm_result != RP_OK) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to set decimation: " << rp_GetError(dcm_result) << "; ";
        std::cout << std::endl;

        return false;
    }

    int poll_timeout = json_number_value(json_object_get(config, "poll_timeout"));

    if (poll_timeout < 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Invalid poll timeout; ";
        std::cout << "Got: " << poll_timeout << "; ";
        std::cout << std::endl;

        global_status.poll_timeout = global_status.base_period;
    } else {
        global_status.poll_timeout = poll_timeout;
    }

    if (global_status.verbosity > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Poll timeout: " << global_status.poll_timeout << " ms; ";
        std::cout << std::endl;
    }

    int rearm_timeout = json_number_value(json_object_get(config, "rearm_timeout"));

    if (rearm_timeout < 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Invalid rearm timeout; ";
        std::cout << "Got: " << rearm_timeout << "; ";
        std::cout << std::endl;

        global_status.rearm_timeout = 0;
    } else {
        global_status.rearm_timeout = rearm_timeout;
    }

    if (global_status.verbosity > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Rearm timeout: " << global_status.rearm_timeout << " ms; ";
        std::cout << std::endl;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Starting the single channels configuration                             //
    ////////////////////////////////////////////////////////////////////////////
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

                rp_channel_t this_channel = RP_CH_1;
                if (id == 0) {
                    this_channel = RP_CH_1;
                } else if (id == 1) {
                    this_channel = RP_CH_2;
                } else {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Channel out of range, ignoring it; ";
                    std::cout << std::endl;
                }
                

                if (enabled) {
                    global_status.enabled_channels.push_back(this_channel);
                }

                json_t *channel_gain_json = json_object_get(value, "gain");
            
                std::string channel_gain;
            
                if (json_is_string(channel_gain_json)) {
                    channel_gain = json_string_value(channel_gain_json);
                } else {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Channel gain is not a string; ";
                    std::cout << std::endl;
                }
            
                rp_pinState_t parsed_channel_gain = RP_HIGH;
            
                if (channel_gain == "low") {
                    parsed_channel_gain = RP_LOW;
                } else if (channel_gain == "high") {
                    parsed_channel_gain = RP_HIGH;
                } else {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Invalid channel gain; ";
                    std::cout << "Got: " << channel_gain << "; ";
                    std::cout << std::endl;
                }
            
                if (global_status.verbosity > 0) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Channel gain: ";
                    if (parsed_channel_gain == RP_LOW) {
                        std::cout << "LOW gain (1 V)";
                    } else if (parsed_channel_gain == RP_HIGH) {
                        std::cout << "HIGH gain (20 V)";
                    } else {
                        std::cout << "Unknown";
                    }
                    std::cout << "; ";
                    std::cout << std::endl;
                }

                const int gain_result = rp_AcqSetGain(this_channel, parsed_channel_gain);

                if (gain_result != RP_OK)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Unable to set channel gain: " << rp_GetError(gain_result) << "; ";
                    std::cout << std::endl;

                    return false;
                }

                if (global_status.verbosity > 0) {
                    rp_pinState_t channel_gain;
                    float channel_max;

                    const int gain_result = rp_AcqGetGain(this_channel, &channel_gain);
                    const int gnvl_result = rp_AcqGetGainV(this_channel, &channel_max);

                    if (gain_result != RP_OK)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "ERROR: Unable to get channel gain in configuration: " << rp_GetError(gain_result) << "; ";
                        std::cout << std::endl;
                    } else if (gnvl_result != RP_OK) {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "ERROR: Unable to get channel gain in volt in configuration: " << rp_GetError(gnvl_result) << "; ";
                        std::cout << std::endl;
                    } else {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "Channel gain: " << channel_max << " V ";
                        if (channel_gain == RP_LOW) {
                            std::cout << "(LOW) ";
                        } else if (channel_gain == RP_HIGH) {
                            std::cout << "(HIGH) ";
                        } else {
                            std::cout << "(UNKNOWN) ";
                        }
                        std::cout << "; ";
                        std::cout << std::endl;
                    }
                }
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // Starting the trigger configuration                                     //
    ////////////////////////////////////////////////////////////////////////////
    json_t *trigger_config = json_object_get(config, "trigger");

    json_t *trigger_mode_json = json_object_get(trigger_config, "mode");

    std::string trigger_mode;

    if (json_is_string(trigger_mode_json)) {
        trigger_mode = json_string_value(trigger_mode_json);
    }

    if (trigger_mode == "auto") {
        global_status.trigger_mode_normal = false;
    } else if (trigger_mode == "normal") {
        global_status.trigger_mode_normal = true;
    } else {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Invalid trigger mode; ";
        std::cout << "Got: " << trigger_mode << "; ";
        std::cout << std::endl;

        global_status.trigger_mode_normal = true;
    }

    if (global_status.verbosity > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Trigger mode: ";
        if (global_status.trigger_mode_normal) {
            std::cout << "Normal";
        } else {
            std::cout << "Auto";
        }
        std::cout << "; ";
        std::cout << std::endl;
    }

    json_t *trigger_source_json = json_object_get(trigger_config, "source");

    std::string trigger_source;

    if (json_is_string(trigger_source_json)) {
        trigger_source = json_string_value(trigger_source_json);
    }

    std::string parsed_trigger_source = "analog";

    if (trigger_source == "analog") {
        parsed_trigger_source = "analog";
    } else if (trigger_source == "external") {
        parsed_trigger_source = "external";
    } else {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Invalid trigger source; ";
        std::cout << "Got: " << trigger_source << "; ";
        std::cout << std::endl;

        parsed_trigger_source = "analog";
    }

    if (global_status.verbosity > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Trigger source: " << parsed_trigger_source << "; ";
        std::cout << std::endl;
    }

    const int trigger_channel = json_integer_value(json_object_get(trigger_config, "channel"));

    rp_channel_t parsed_trigger_channel = RP_CH_1;

    if (parsed_trigger_source == "analog" && 0 <= trigger_channel && trigger_channel < 2) {
        if (trigger_channel == 0) {
            parsed_trigger_channel = RP_CH_1;
        } else if (trigger_channel == 1) {
            parsed_trigger_channel = RP_CH_2;
        }
    } else {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Invalid trigger channel; ";
        std::cout << "Got: " << trigger_channel << "; ";
        std::cout << std::endl;
    }

    if (global_status.verbosity > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Trigger channel: " << parsed_trigger_channel << "; ";
        std::cout << std::endl;
    }

    json_t *trigger_slope_json = json_object_get(trigger_config, "slope");

    std::string trigger_slope;

    if (json_is_string(trigger_slope_json)) {
        trigger_slope = json_string_value(trigger_slope_json);
    }

    std::string parsed_trigger_slope = "falling";

    if (trigger_slope == "rising") {
        parsed_trigger_slope = "rising";
    } else if (trigger_slope == "falling") {
        parsed_trigger_slope = "falling";
    } else {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Invalid trigger slope; ";
        std::cout << "Got: " << trigger_slope << "; ";
        std::cout << std::endl;
    }

    if (global_status.verbosity > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Trigger slope: " << parsed_trigger_slope << "; ";
        std::cout << std::endl;
    }

    // Setting the trigger source using all the values
    // Default value:
    global_status.trigger_source = RP_TRIG_SRC_CHA_NE;

    if (parsed_trigger_source == "analog"
        && parsed_trigger_channel == RP_CH_1
        && parsed_trigger_slope == "falling")
    {
        global_status.trigger_source = RP_TRIG_SRC_CHA_NE;
    }
    else if (parsed_trigger_source == "analog"
             && parsed_trigger_channel == RP_CH_1
             && parsed_trigger_slope == "rising")
    {
        global_status.trigger_source = RP_TRIG_SRC_CHA_PE;
    }
    else if (parsed_trigger_source == "analog"
             && parsed_trigger_channel == RP_CH_2
             && parsed_trigger_slope == "falling")
    {
        global_status.trigger_source = RP_TRIG_SRC_CHB_NE;
    }
    else if (parsed_trigger_source == "analog"
             && parsed_trigger_channel == RP_CH_2
             && parsed_trigger_slope == "rising")
    {
        global_status.trigger_source = RP_TRIG_SRC_CHB_PE;
    }
    else if (parsed_trigger_source == "external"
             && parsed_trigger_slope == "falling")
    {
        global_status.trigger_source = RP_TRIG_SRC_EXT_NE;
    }
    else if (parsed_trigger_source == "external"
             && parsed_trigger_slope == "rising")
    {
        global_status.trigger_source = RP_TRIG_SRC_EXT_PE;
    }

    // This actually starts the acquisition of triggered data after a rp_AcqStart()
    const int trgsrc_result = rp_AcqSetTriggerSrc(global_status.trigger_source);

    if (trgsrc_result != RP_OK)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to set trigger source: " << rp_GetError(trgsrc_result) << "; ";
        std::cout << std::endl;

        return false;
    }

    if (global_status.verbosity > 0) {
        rp_acq_trig_src_t trigger_source;

        const int trgsrc_result = rp_AcqGetTriggerSrc(&trigger_source);

        if (trgsrc_result != RP_OK)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR: Unable to get trigger source in configuration: " << rp_GetError(trgsrc_result) << "; ";
            std::cout << std::endl;
        } else {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Trigger source: ";
            if (trigger_source == RP_TRIG_SRC_DISABLED) {
                std::cout << "Disabled";
            } else if (trigger_source == RP_TRIG_SRC_NOW) {
                std::cout << "Triggered now";
            } else if (trigger_source == RP_TRIG_SRC_CHA_PE) {
                std::cout << "Channel 0, threshold, rising edge";
            } else if (trigger_source == RP_TRIG_SRC_CHA_NE) {
                std::cout << "Channel 0, threshold, falling edge";
            } else if (trigger_source == RP_TRIG_SRC_CHB_PE) {
                std::cout << "Channel 1, threshold, rising edge";
            } else if (trigger_source == RP_TRIG_SRC_CHB_NE) {
                std::cout << "Channel 1, threshold, falling edge";
            } else if (trigger_source == RP_TRIG_SRC_EXT_PE) {
                std::cout << "External, threshold, rising edge";
            } else if (trigger_source == RP_TRIG_SRC_EXT_NE) {
                std::cout << "External, threshold, falling edge";
            } else if (trigger_source == RP_TRIG_SRC_AWG_PE) {
                std::cout << "Wave generator, threshold, rising edge";
            } else if (trigger_source == RP_TRIG_SRC_AWG_NE) {
                std::cout << "Wave generator, threshold, falling edge";
            } else {
                std::cout << "Unknown";
            }
            
            std::cout << " (" << trigger_source << "); ";
            std::cout << std::endl;
        }
    }

    float trigger_level = json_number_value(json_object_get(trigger_config, "level"));

    rp_pinState_t trigger_channel_gain;
    float trigger_channel_max;

    const int gain_result = rp_AcqGetGain(parsed_trigger_channel, &trigger_channel_gain);
    const int gnvl_result = rp_AcqGetGainV(parsed_trigger_channel, &trigger_channel_max);

    if (gain_result != RP_OK)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to get channel gain in configuration: " << rp_GetError(gain_result) << "; ";
        std::cout << std::endl;
    }

    if (gnvl_result != RP_OK)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to get channel gain in volt in configuration: " << rp_GetError(gnvl_result) << "; ";
        std::cout << std::endl;
    }

    if (trigger_level < -trigger_channel_max || trigger_channel_max < trigger_level)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Invalid trigger level; ";
        std::cout << "Got: " << trigger_level << "; ";
        std::cout << std::endl;

        trigger_level = trigger_channel_max / 2.0;
    }

    const int trglvl_result = rp_AcqSetTriggerLevel(parsed_trigger_channel, trigger_level);

    if (trglvl_result != RP_OK) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to set trigger level: " << rp_GetError(trglvl_result) << "; ";
        std::cout << std::endl;

        return false;
    }

    if (global_status.verbosity > 0) {
        float trigger_level = 0;

        const int trglvl_result = rp_AcqGetTriggerLevel(&trigger_level);

        if (trglvl_result != RP_OK)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR: Unable to get trigger level in configuration: " << rp_GetError(trglvl_result) << "; ";
            std::cout << std::endl;
        } else {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Trigger level: " << trigger_level << "; ";
            std::cout << std::endl;
        }
    }

    const int32_t pretrigger = json_integer_value(json_object_get(trigger_config, "pretrigger"));

    if ((-pretrigger) >= ADC_BUFFER_SIZE || pretrigger >= ADC_BUFFER_SIZE) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Invalid pretrigger; ";
        std::cout << "Got: " << static_cast<int>(pretrigger) << "; ";
        std::cout << std::endl;

        global_status.pretrigger = defaults_abrp_pretrigger;
    } else {
        global_status.pretrigger = pretrigger;
    }

    int32_t trigger_delay = global_status.samples_number - global_status.pretrigger;

    if (trigger_delay >= ADC_BUFFER_SIZE || trigger_delay < 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Invalid trigger delay; ";
        std::cout << "Got: " << (int)trigger_delay << "; ";
        std::cout << std::endl;

        trigger_delay = defaults_abrp_scope_samples - defaults_abrp_pretrigger;
    }

    global_status.trigger_delay = trigger_delay;

    const int trgdly_result = rp_AcqSetTriggerDelay(trigger_delay);

    if (trgdly_result != RP_OK)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to set trigger delay: " << rp_GetError(trgdly_result) << "; ";
        std::cout << std::endl;
    }

    if (global_status.verbosity > 0) {
        int32_t trigger_delay = 0;

        const int trgdly_result = rp_AcqGetTriggerDelay(&trigger_delay);

        if (trgdly_result != RP_OK)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR: Unable to get trigger delay in configuration: " << rp_GetError(trgdly_result) << "; ";
            std::cout << std::endl;
        } else {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Trigger delay: " << static_cast<int>(trigger_delay) << "; ";
            std::cout << std::endl;
        }
    }

    // ...so, this is not what I thought it was...
    // Do not use it!
    // Actually, better set it to false, just in case something else sets it.
    const int akp_result = rp_AcqSetArmKeep(false);

    if (akp_result != RP_OK) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to enable continuous acquisition: " << rp_GetError(akp_result) << "; ";
        std::cout << std::endl;
    }

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Red Pitaya configuration completed successfully!";
        std::cout << std::endl;
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

    if (global_status.verbosity > 1) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Receiving command... ";
        std::cout << std::endl;
    }

    json_t *json_message = NULL;

    const int result = receive_json_message(commands_socket, NULL, &json_message, false, global_status.verbosity);

    if (!json_message || result == EXIT_FAILURE)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Error on receiving JSON commands message";
        std::cout << std::endl;
    }
    else
    {
        const size_t command_ID = json_integer_value(json_object_get(json_message, "msg_ID"));

        if (global_status.verbosity > 1) {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Received command; ";
            std::cout << "Command ID: " << command_ID << "; ";
            std::cout << std::endl;
        }

        json_t *json_command = json_object_get(json_message, "command");
        json_t *json_arguments = json_object_get(json_message, "arguments");

        if (json_command != NULL)
        {
            const std::string command = json_string_value(json_command);

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Received command: " << command << "; ";
                std::cout << std::endl;
            }

            if (command == std::string("start")) {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "################################################################### Start!!! ###";
                std::cout << std::endl;

                return states::START_ACQUISITION;
            } else if (command == std::string("reconfigure") && json_arguments) {
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
            } else if (command == std::string("off")) {
                return states::CLEAR_MEMORY;
            } else if (command == std::string("quit")) {
                return states::CLEAR_MEMORY;
            }
        }
    }

    json_decref(json_message);

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    if (now - global_status.last_publication > std::chrono::seconds(defaults_abcd_publish_timeout))
    {
        return states::PUBLISH_STATUS;
    }

    return states::RECEIVE_COMMANDS;
}

state actions::publish_status(status &global_status)
{
    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Publishing status; ";
        std::cout << std::endl;
    }

    json_t *status_message = json_object();

    json_object_set_new_nocheck(status_message, "config", json_deep_copy(global_status.config));

    json_t *acquisition = json_object();
    json_object_set_new_nocheck(acquisition, "running", json_false());
    json_object_set_new_nocheck(status_message, "acquisition", acquisition);

    json_t *digitizer = json_object();

    // Apparently there is no way to request a status from the Red Pitaya
    // we can only assume that everything is OK
    //if ...
    //    json_object_set_new_nocheck(digitizer, "valid_pointer", json_false());
    //    json_object_set_new_nocheck(digitizer, "active", json_false());
    //} else {
    //    json_object_set_new_nocheck(digitizer, "valid_pointer", json_true());
    //    json_object_set_new_nocheck(digitizer, "active", json_true());
    //}
    json_object_set_new_nocheck(digitizer, "valid_pointer", json_true());
    json_object_set_new_nocheck(digitizer, "active", json_true());

    json_object_set_new_nocheck(status_message, "digitizer", digitizer);

    actions::generic::publish_message(global_status, defaults_abcd_status_topic, status_message);

    json_decref(status_message);

    return states::RECEIVE_COMMANDS;
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
    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Enabling continuous acquisition; ";
        std::cout << std::endl;
    }

    global_status.start_time = start_time;

    return states::ACQUISITION_RECEIVE_COMMANDS;
}

state actions::acquisition_receive_commands(status &global_status)
{
    void *commands_socket = global_status.commands_socket;

    if (global_status.verbosity > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Receiving command... ";
        std::cout << std::endl;
    }

    json_t *json_message = NULL;

    const int result = receive_json_message(commands_socket, NULL, &json_message, false, global_status.verbosity);

    if (!json_message || result == EXIT_FAILURE)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Error on receiving JSON commands message";
        std::cout << std::endl;
    }
    else
    {
        const size_t command_ID = json_integer_value(json_object_get(json_message, "msg_ID"));

        if (global_status.verbosity > 1) {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Received command; ";
            std::cout << "Command ID: " << command_ID << "; ";
            std::cout << std::endl;
        }

        json_t *json_command = json_object_get(json_message, "command");

        if (json_command != NULL)
        {
            const std::string command = json_string_value(json_command);

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Received command: " << command << "; ";
                std::cout << std::endl;
            }

            if (command == std::string("stop")) {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "#################################################################### Stop!!! ###";
                std::cout << std::endl;

                return states::STOP_PUBLISH_EVENTS;
            }
        }

        json_decref(json_message);
        
    }

    return states::ADD_TO_BUFFER;
}

state actions::add_to_buffer(status &global_status)
{
    const unsigned int verbosity = global_status.verbosity;

    const auto before_inner_start = std::chrono::system_clock::now();
    const auto publish_timeout = std::chrono::seconds(defaults_abcd_publish_timeout);
    unsigned int inner_loop_counter = 0;

    // Starting a very quick inner loop to increase the rate
    while (std::chrono::system_clock::now() - before_inner_start < publish_timeout)
    {
        if (verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Inner loop counter: " << inner_loop_counter << "; ";
            std::cout << std::endl;
        }

        const std::chrono::time_point<std::chrono::system_clock> before_start = std::chrono::system_clock::now();

        // Restart acquisition after event
        if (verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Restarting acquistion; ";
            std::cout << std::endl;
        }
        const int rst_result = rp_AcqStart();

        if (verbosity > 0)
        {
            const std::chrono::time_point<std::chrono::system_clock> after_restart = std::chrono::system_clock::now();
            const auto restart_duration = std::chrono::duration_cast<nanoseconds>(after_restart - before_start).count();

            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Restart duration: " << restart_duration * 1e-6 << " ms; ";
            std::cout << 1.0 / restart_duration * 1e9 << " Hz; ";
            std::cout << std::endl;
        }

        if (rst_result != RP_OK) {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR: Unable to start acquisition: " << rp_GetError(rst_result) << "; ";
            std::cout << std::endl;
        }

        //// pretrigger is in samples so we divide it by the acquisition rate
        //const unsigned int fill_delay = std::ceil(global_status.pretrigger / 125e6 * 1e6);

        //if (verbosity > 0)
        //{
        //    char time_buffer[BUFFER_SIZE];
        //    time_string(time_buffer, BUFFER_SIZE, NULL);
        //    std::cout << '[' << time_buffer << "] ";
        //    std::cout << "Waiting for: " << fill_delay << " us; ";
        //    std::cout << std::endl;
        //}

        //// After acquisition is started some time delay is needed
        //// in order to acquire fresh samples in to buffer
        //std::this_thread::sleep_for(std::chrono::microseconds(fill_delay));

        // Rearm trigger
        if (verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Rearming trigger; ";
            std::cout << std::endl;
        }

        const int trgsrc_result = rp_AcqSetTriggerSrc(global_status.trigger_source);

        const std::chrono::time_point<std::chrono::system_clock> after_rearm = std::chrono::system_clock::now();
        const double rearm_duration = std::chrono::duration_cast<nanoseconds>(after_rearm - before_start).count() * 1e-6;

        if (verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Rearm duration: " << rearm_duration << " ms; ";
            std::cout << 1.0 / rearm_duration * 1e3 << " Hz; ";
            std::cout << std::endl;
        }

        if (trgsrc_result != RP_OK)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR: Unable to set trigger source inside acquisition loop: " << rp_GetError(trgsrc_result) << "; ";
            std::cout << std::endl;

            return states::ACQUISITION_ERROR;
        }

        // If the rearming procedure is too long it fails,
        // therefore we try again.
        if (global_status.rearm_timeout > 0 && rearm_duration >= global_status.rearm_timeout) {
            if (verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "WARNING: Rearm duration was too long, skipping event; ";
                std::cout << std::endl;
            }
        } else {

            // Check trigger state, either it is waiting for a trigger to happen,
            // or it has already been triggered.
            rp_acq_trig_state_t trigger_state = RP_TRIG_STATE_WAITING;

            const std::chrono::time_point<std::chrono::system_clock> before_poll = std::chrono::system_clock::now();
            const auto poll_timeout = std::chrono::milliseconds(global_status.poll_timeout);

            do
            {
                const int trgstt_result = rp_AcqGetTriggerState(&trigger_state);

                if (trgstt_result != RP_OK) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Unable to get trigger state in acquisition loop: " << rp_GetError(trgstt_result) << "; ";
                    std::cout << std::endl;

                    // FIXME: I am not sure what to do here

                    return states::ACQUISITION_ERROR;
                }
            }
            while ((std::chrono::system_clock::now() - before_poll < poll_timeout)
                    && (trigger_state == RP_TRIG_STATE_WAITING));

            if (std::chrono::system_clock::now() - before_poll > poll_timeout)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "WARNING: Poll timeout, skipping event; ";
                std::cout << std::endl;

                if (!global_status.trigger_mode_normal) {
                    // Trigger now!
                    const int trgset_result = rp_AcqSetTriggerSrc(RP_TRIG_SRC_NOW);

                    if (trgset_result != RP_OK) {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "ERROR in AcqSetTriggerSrc inside acquisition loop";
                        std::cout << "ERROR: Unable to trigger now in acquisition loop: " << rp_GetError(trgset_result) << "; ";
                        std::cout << std::endl;
                    }

                    const int trgstt_result = rp_AcqGetTriggerState(&trigger_state);

                    if (trgstt_result != RP_OK) {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "ERROR: Unable to get trigger state in acquisition loop: " << rp_GetError(trgstt_result) << "; ";
                        std::cout << std::endl;
                    }
                }
            }

            if (verbosity > 0)
            {
                const std::chrono::time_point<std::chrono::system_clock> after_poll = std::chrono::system_clock::now();
                const auto poll_duration = std::chrono::duration_cast<nanoseconds>(after_poll - before_poll).count();

                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Poll duration: " << poll_duration * 1e-6 << " ms; ";
                std::cout << 1.0 / poll_duration * 1e9 << " Hz; ";
                std::cout << std::endl;
            }

            if (verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Trigger state: ";
                if (trigger_state == RP_TRIG_STATE_WAITING) {
                    std::cout << "Waiting";
                } else if (trigger_state == RP_TRIG_STATE_TRIGGERED) {
                    std::cout << "Triggered";
                } else {
                    std::cout << "Unknown";
                }
                std::cout << "; ";
                std::cout << std::endl;
            }

            if (trigger_state == RP_TRIG_STATE_TRIGGERED)
            {
                if (verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Downloading samples; ";
                    std::cout << std::endl;
                }

                const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
                const auto Delta = std::chrono::duration_cast<nanoseconds>(now - global_status.start_time);
                const uint64_t timestamp_ns = Delta.count();
                // We define the timestamp in the same unit as the clock samples (8 ns)
                // so the following analysis can calculate fine timestamps in the same 
                // unit of time.
                const uint64_t timestamp = timestamp_ns / (global_status.decimation * 8);

                // Get position of ADC write pointer at time when trigger arrived.
                uint32_t trigger_position = 0;

                const int trgpos_result = rp_AcqGetWritePointerAtTrig(&trigger_position);

                if (trgpos_result != RP_OK)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Unable to get trigger position in acquisition loop: " << rp_GetError(trgpos_result) << "; ";
                    std::cout << std::endl;

                    // FIXME: I am not sure what to do here

                    return states::ACQUISITION_ERROR;
                }

                if (verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Trigger position: " << static_cast<unsigned int>(trigger_position) << "; ";
                    std::cout << std::endl;
                }

                int32_t trigger_delay = 0;

                const int trgdly_result = rp_AcqGetTriggerDelay(&trigger_delay);

                if (trgdly_result != RP_OK)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Unable to get trigger delay in acquisition loop: " << rp_GetError(trgdly_result) << "; ";
                    std::cout << std::endl;

                    // FIXME: I am not sure what to do here

                    return states::ACQUISITION_ERROR;
                }

                if (verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Trigger delay: " << static_cast<int>(trigger_delay) << "; ";
                    std::cout << std::endl;
                }

                int32_t start_position = trigger_position + trigger_delay - global_status.samples_number;

                if (verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Trigger position: " << static_cast<unsigned int>(trigger_position) << "; ";
                    std::cout << "Trigger delay: " << static_cast<int>(trigger_delay) << "; ";
                    std::cout << "Pretrigger: " << static_cast<int>(global_status.pretrigger) << "; ";
                    std::cout << "Samples number: " << static_cast<int>(global_status.samples_number) << "; ";
                    std::cout << "Start position: " << static_cast<int>(start_position) << "; ";
                    std::cout << std::endl;
                }

                std::vector<int16_t> raw_samples(global_status.samples_number);

                for (auto& channel : global_status.enabled_channels) {
                    if (verbosity > 0) {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "Downloading samples from channel: " << channel << "; ";
                        std::cout << std::endl;
                    }

                    const std::chrono::time_point<std::chrono::system_clock> data_reading_start = std::chrono::system_clock::now();

                    uint32_t read_left_size = global_status.samples_number;

                    // Apparently this function treats correctly the circular buffer feature
                    const int lftdat_result = rp_AcqGetDataRaw(channel, start_position, &read_left_size, raw_samples.data());

                    if (verbosity > 0)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "Filled size of left buffer: " << read_left_size << "; ";
                        std::cout << std::endl;
                    }

                    if (lftdat_result != RP_OK)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "ERROR: Unable to get left raw data in acquisition loop: " << rp_GetError(lftdat_result) << "; ";
                        std::cout << std::endl;

                        // FIXME: I am not sure what to do here

                        return states::ACQUISITION_ERROR;
                    }

                    if (verbosity > 0)
                    {
                        const std::chrono::time_point<std::chrono::system_clock> data_reading_end = std::chrono::system_clock::now();
                        const auto data_reading_duration = std::chrono::duration_cast<nanoseconds>(data_reading_end - data_reading_start);

                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "Data reading duration: " << data_reading_duration.count() * 1e-6 << " ms; ";
                        std::cout << 1.0 / data_reading_duration.count() * 1e9 << " Hz; ";
                        std::cout << std::endl;
                    }

                    global_status.counts[channel] += 1;
                    global_status.partial_counts[channel] += 1;

                    global_status.waveforms_buffer.emplace_back(timestamp, channel, raw_samples.size());

                    // Trasforming the signed int to an unsigned int for compatibility with the rest of ABCD
                    for (uint32_t j = 0; j < raw_samples.size(); j++) {
                        global_status.waveforms_buffer.back().samples[j] = raw_samples[j] + (1 << 15);
                    }
                }

                if (verbosity > 0) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Waveforms buffer size: " << global_status.waveforms_buffer.size() << "; ";
                    std::cout << std::endl;
                }
                if (global_status.waveforms_buffer.size() >= global_status.events_buffer_max_size) {
                    return states::PUBLISH_EVENTS;
                }
            }
        }

        inner_loop_counter += 1;
    }

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();

    if (now - global_status.last_publication > std::chrono::seconds(defaults_abcd_publish_timeout)) {
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
    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Publishing status; ";
        std::cout << std::endl;
    }

    json_t *status_message = json_object();

    json_object_set_new_nocheck(status_message, "config", json_deep_copy(global_status.config));

    json_t *acquisition = json_object();
    json_t *digitizer = json_object();

    // Apparently there is no way to request a status from the Red Pitaya
    // we can only assume that everything is OK
    if (true) {
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

    return states::ACQUISITION_RECEIVE_COMMANDS;
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
    actions::generic::clear_memory(global_status);

    return states::DESTROY_DIGITIZER;
}

state actions::reconfigure_clear_memory(status &global_status)
{
    actions::generic::clear_memory(global_status);

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
    actions::generic::clear_memory(global_status);

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
