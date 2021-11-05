/*
 * (C) Copyright 2021, European Union, Cristiano Lino Fontana
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

#include <cstring>
#include <chrono>
#include <unistd.h>
#include <iostream>
#include <string>
#include <set>
#include <vector>
#include <map>
#include <cstdint>
#include <ctime>
#include <limits>
#include <thread>
#include <algorithm>

extern "C" {
#include <zmq.h>
#include <jansson.h>
// For PRIu8 and PRIu32
#include <inttypes.h>
}

#include "typedefs.hpp"
#include "states.hpp"
#include "actions.hpp"

#define counter_type uint64_t

extern "C" {
#include "defaults.h"
#include "utilities_functions.h"
#include "socket_functions.h"
#include "jansson_socket_functions.h"
#include "events.h"
#include "analysis_functions.h"
}

#define BUFFER_SIZE 32

/******************************************************************************/
/* Generic actions                                                            */
/******************************************************************************/

void actions::generic::publish_message(status &global_status,
                                       std::string topic,
                                       json_t *status_message)
{
    void *status_socket = global_status.status_socket;
    const unsigned long int status_msg_ID = global_status.status_msg_ID;

    char time_buffer[BUFFER_SIZE];
    time_string(time_buffer, BUFFER_SIZE, NULL);

    json_object_set_new(status_message, "module", json_string("waan"));
    json_object_set_new(status_message, "timestamp", json_string(time_buffer));
    json_object_set_new(status_message, "msg_ID", json_integer(status_msg_ID));

    send_json_message(status_socket, const_cast<char*>(topic.c_str()), status_message, 1);

    global_status.status_msg_ID += 1;
}

void actions::generic::clear_memory(status &global_status)
{
    if (global_status.verbosity > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Cleaning up memory; ";
        std::cout << std::endl;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Cleaning up the memory and configurations                              //
    ////////////////////////////////////////////////////////////////////////////
    global_status.partial_counts.clear();
    global_status.channels_timestamp_init.clear();
    global_status.channels_timestamp_analysis.clear();
    global_status.channels_energy_init.clear();
    global_status.channels_energy_analysis.clear();

    // We separately close the user configs because they might not have been
    // both initialized, the user should take care of not freeing the NULL pointer.
    for (auto& pair : global_status.channels_timestamp_user_config) {
        const unsigned int id = pair.first;
        void *const timestamp_config = pair.second;

        if (global_status.verbosity > 0) {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Calling user close on the timestamp_user_config; ";
            std::cout << "Channel: " << id << "; ";
            std::cout << std::endl;
        }

        global_status.channels_timestamp_close[id].fn(timestamp_config);
    }
    global_status.channels_timestamp_user_config.clear();

    global_status.channels_timestamp_close.clear();

    for (auto& pair : global_status.channels_energy_user_config) {
        const unsigned int id = pair.first;
        void *const energy_config = pair.second;

        if (global_status.verbosity > 0) {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Calling user close on the energy_user_config; ";
            std::cout << "Channel: " << id << "; ";
            std::cout << std::endl;
        }

        global_status.channels_energy_close[id].fn(energy_config);
    }
    global_status.channels_energy_user_config.clear();

    global_status.channels_energy_close.clear();

    for (auto& pair : global_status.dl_timestamp_handles) {
        void *const dl_handle = pair.second;

        if (dl_handle) {
            dlclose(dl_handle);
        }
    }
    global_status.dl_timestamp_handles.clear();

    for (auto& pair : global_status.dl_energy_handles) {
        void *const dl_handle = pair.second;

        if (dl_handle) {
            dlclose(dl_handle);
        }
    }
    global_status.dl_energy_handles.clear();

    global_status.active_channels.clear();
    global_status.disabled_channels.clear();
}

bool actions::generic::configure(status &global_status)
{
    unsigned int verbosity = global_status.verbosity;

    if (global_status.verbosity > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Configuring waan; ";
        std::cout << std::endl;
    }

    actions::generic::clear_memory(global_status);

    json_t *config = global_status.config;

    ////////////////////////////////////////////////////////////////////////////
    // Starting the global configuration                                      //
    ////////////////////////////////////////////////////////////////////////////
    if (global_status.verbosity > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Global configuration; ";
        std::cout << std::endl;
    }

    global_status.forward_waveforms = json_is_true(json_object_get(config, "forward_waveforms"));
    global_status.enable_additional = json_is_true(json_object_get(config, "enable_additional"));

    const bool discard_messages = json_is_true(json_object_get(config, "discard_messages"));

    if (discard_messages) {
        const int conflate = discard_messages ? 1 : 0;

        zmq_setsockopt(global_status.data_input_socket,
                       ZMQ_CONFLATE,
                       &conflate,
                       sizeof(conflate));

        //const int high_water_mark = 10;

        //zmq_setsockopt(global_status.data_input_socket,
        //               ZMQ_RCVHWM,
        //               &high_water_mark,
        //               sizeof(high_water_mark));
    }

    if (global_status.verbosity > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Forward waveforms: " << (global_status.forward_waveforms ? "true" : "false") << "; ";
        std::cout << std::endl;
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Enable additional: " << (global_status.enable_additional ? "true" : "false") << "; ";
        std::cout << std::endl;
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Discarding messages: " << (discard_messages ? "true" : "false") << "; ";
        std::cout << std::endl;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Starting the single channels configuration                             //
    ////////////////////////////////////////////////////////////////////////////
    if (global_status.verbosity > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Channels configuration; ";
        std::cout << std::endl;
    }

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

                const bool enabled = json_is_true(json_object_get(value, "enable"))
                                     || json_is_true(json_object_get(value, "enabled"));

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

                bool dl_loading_error = false;

                json_t *libraries_json = json_object_get(value, "user_libraries");
            
                ////////////////////////////////////////////////////////////////
                // Libraries loading                                          //
                ////////////////////////////////////////////////////////////////
                if (!enabled) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Not loading libraries for disabled channel; ";
                    std::cout << std::endl;

                    dl_loading_error = true;
                } else if (!json_is_object(libraries_json)) {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: user_libraries is not an object; ";
                    std::cout << std::endl;

                    dl_loading_error = true;
                } else {
                    const std::string lib_timestamp =
                        json_string_value(json_object_get(libraries_json, "timestamp"));
                    const std::string lib_energy =
                        json_string_value(json_object_get(libraries_json, "energy"));

                    if (verbosity > 0)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "Library for timestamp: " << lib_timestamp << "; ";
                        std::cout << std::endl;
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "Library for energy:    " << lib_energy << "; ";
                        std::cout << std::endl;
                    }

                    ////////////////////////////////////////////////////////////
                    // Timestamp analysis library                             //
                    ////////////////////////////////////////////////////////////
                    if (lib_timestamp.length() == 0) {
                        if (verbosity > 0)
                        {
                            char time_buffer[BUFFER_SIZE];
                            time_string(time_buffer, BUFFER_SIZE, NULL);
                            std::cout << '[' << time_buffer << "] ";
                            std::cout << "ERROR: Empty timestamp library; ";
                            std::cout << std::endl;
                        }

                        dl_loading_error = true;
                    } else {
                        if (verbosity > 0)
                        {
                            char time_buffer[BUFFER_SIZE];
                            time_string(time_buffer, BUFFER_SIZE, NULL);
                            std::cout << '[' << time_buffer << "] ";
                            std::cout << "Loading library: " << lib_timestamp << "; ";
                            std::cout << std::endl;
                        }

                        void *dl_handle = dlopen(lib_timestamp.c_str(), RTLD_NOW);

                        if (!dl_handle) {
                            char time_buffer[BUFFER_SIZE];
                            time_string(time_buffer, BUFFER_SIZE, NULL);
                            std::cout << '[' << time_buffer << "] ";
                            std::cout << "ERROR: Unable to load timestamp library: " << dlerror() << "; ";
                            std::cout << std::endl;

                            dl_loading_error = true;
                        } else {
                            if (verbosity > 0)
                            {
                                char time_buffer[BUFFER_SIZE];
                                time_string(time_buffer, BUFFER_SIZE, NULL);
                                std::cout << '[' << time_buffer << "] ";
                                std::cout << "Loading timestamp_init() function; ";
                                std::cout << std::endl;
                            }

                            void *dl_init = dlsym(dl_handle, "timestamp_init");

                            if (!dl_init) {
                                char time_buffer[BUFFER_SIZE];
                                time_string(time_buffer, BUFFER_SIZE, NULL);
                                std::cout << '[' << time_buffer << "] ";
                                std::cout << "ERROR: Unable to load timestamp init function: " << dlerror() << "; ";
                                std::cout << std::endl;

                                global_status.channels_timestamp_init[id].fn = dummy_init;
                            } else {
                                global_status.channels_timestamp_init[id].obj = dl_init;
                            }

                            if (verbosity > 0)
                            {
                                char time_buffer[BUFFER_SIZE];
                                time_string(time_buffer, BUFFER_SIZE, NULL);
                                std::cout << '[' << time_buffer << "] ";
                                std::cout << "Loading timestamp_close() function; ";
                                std::cout << std::endl;
                            }

                            void *dl_close = dlsym(dl_handle, "timestamp_close");

                            if (!dl_close) {
                                char time_buffer[BUFFER_SIZE];
                                time_string(time_buffer, BUFFER_SIZE, NULL);
                                std::cout << '[' << time_buffer << "] ";
                                std::cout << "ERROR: Unable to load timestamp close function: " << dlerror() << "; ";
                                std::cout << std::endl;

                                global_status.channels_timestamp_close[id].fn = dummy_close;
                            } else {
                                global_status.channels_timestamp_close[id].obj = dl_close;
                            }

                            if (verbosity > 0)
                            {
                                char time_buffer[BUFFER_SIZE];
                                time_string(time_buffer, BUFFER_SIZE, NULL);
                                std::cout << '[' << time_buffer << "] ";
                                std::cout << "Loading timestamp_analysis() function; ";
                                std::cout << std::endl;
                            }

                            void *dl_timestamp = dlsym(dl_handle, "timestamp_analysis");

                            if (!dl_timestamp) {
                                char time_buffer[BUFFER_SIZE];
                                time_string(time_buffer, BUFFER_SIZE, NULL);
                                std::cout << '[' << time_buffer << "] ";
                                std::cout << "ERROR: Unable to load timestamp function: " << dlerror() << "; ";
                                std::cout << std::endl;

                                dl_loading_error = true;
                            }

                            if (!dl_loading_error) {
                                if (verbosity > 0)
                                {
                                    char time_buffer[BUFFER_SIZE];
                                    time_string(time_buffer, BUFFER_SIZE, NULL);
                                    std::cout << '[' << time_buffer << "] ";
                                    std::cout << "Successfully loaded the functions; ";
                                    std::cout << std::endl;
                                }

                                global_status.dl_timestamp_handles[id] = dl_handle;
                                global_status.channels_timestamp_analysis[id].obj = dl_timestamp;
                            } else {
                                if (verbosity > 0)
                                {
                                    char time_buffer[BUFFER_SIZE];
                                    time_string(time_buffer, BUFFER_SIZE, NULL);
                                    std::cout << '[' << time_buffer << "] ";
                                    std::cout << "ERROR: unable to load the functions; ";
                                    std::cout << std::endl;
                                }
                            }
                        }
                    }

                    ////////////////////////////////////////////////////////////
                    // Energy analysis library                                //
                    ////////////////////////////////////////////////////////////
                    if (lib_energy.length() == 0) {
                        if (verbosity > 0)
                        {
                            char time_buffer[BUFFER_SIZE];
                            time_string(time_buffer, BUFFER_SIZE, NULL);
                            std::cout << '[' << time_buffer << "] ";
                            std::cout << "ERROR: Empty energy library; ";
                            std::cout << std::endl;
                        }

                        dl_loading_error = true;
                    } else {
                        if (verbosity > 0)
                        {
                            char time_buffer[BUFFER_SIZE];
                            time_string(time_buffer, BUFFER_SIZE, NULL);
                            std::cout << '[' << time_buffer << "] ";
                            std::cout << "Loading library: " << lib_energy << "; ";
                            std::cout << std::endl;
                        }

                        void *dl_handle = dlopen(lib_energy.c_str(), RTLD_NOW);

                        if (!dl_handle) {
                            char time_buffer[BUFFER_SIZE];
                            time_string(time_buffer, BUFFER_SIZE, NULL);
                            std::cout << '[' << time_buffer << "] ";
                            std::cout << "ERROR: Unable to load energy library: " << dlerror() << "; ";
                            std::cout << std::endl;

                            dl_loading_error = true;
                        } else {
                            if (verbosity > 0)
                            {
                                char time_buffer[BUFFER_SIZE];
                                time_string(time_buffer, BUFFER_SIZE, NULL);
                                std::cout << '[' << time_buffer << "] ";
                                std::cout << "Loading energy_init() function; ";
                                std::cout << std::endl;
                            }

                            void *dl_init = dlsym(dl_handle, "energy_init");

                            if (!dl_init) {
                                char time_buffer[BUFFER_SIZE];
                                time_string(time_buffer, BUFFER_SIZE, NULL);
                                std::cout << '[' << time_buffer << "] ";
                                std::cout << "ERROR: Unable to load energy init function: " << dlerror() << "; ";
                                std::cout << std::endl;

                                global_status.channels_energy_init[id].fn = dummy_init;
                            } else {
                                global_status.channels_energy_init[id].obj = dl_init;
                            }

                            if (verbosity > 0)
                            {
                                char time_buffer[BUFFER_SIZE];
                                time_string(time_buffer, BUFFER_SIZE, NULL);
                                std::cout << '[' << time_buffer << "] ";
                                std::cout << "Loading energy_close() function; ";
                                std::cout << std::endl;
                            }

                            void *dl_close = dlsym(dl_handle, "energy_close");

                            if (!dl_close) {
                                char time_buffer[BUFFER_SIZE];
                                time_string(time_buffer, BUFFER_SIZE, NULL);
                                std::cout << '[' << time_buffer << "] ";
                                std::cout << "ERROR: Unable to load energy close function: " << dlerror() << "; ";
                                std::cout << std::endl;

                                global_status.channels_energy_close[id].fn = dummy_close;
                            } else {
                                global_status.channels_energy_close[id].obj = dl_close;
                            }

                            if (verbosity > 0)
                            {
                                char time_buffer[BUFFER_SIZE];
                                time_string(time_buffer, BUFFER_SIZE, NULL);
                                std::cout << '[' << time_buffer << "] ";
                                std::cout << "Loading energy_analysis() function; ";
                                std::cout << std::endl;
                            }

                            void *dl_energy = dlsym(dl_handle, "energy_analysis");

                            if (!dl_energy) {
                                char time_buffer[BUFFER_SIZE];
                                time_string(time_buffer, BUFFER_SIZE, NULL);
                                std::cout << '[' << time_buffer << "] ";
                                std::cout << "ERROR: Unable to load energy function: " << dlerror() << "; ";
                                std::cout << std::endl;

                                dl_loading_error = true;
                            }

                            if (!dl_loading_error) {
                                if (verbosity > 0)
                                {
                                    char time_buffer[BUFFER_SIZE];
                                    time_string(time_buffer, BUFFER_SIZE, NULL);
                                    std::cout << '[' << time_buffer << "] ";
                                    std::cout << "Successfully loaded the functions; ";
                                    std::cout << std::endl;
                                }

                                global_status.dl_energy_handles[id] = dl_handle;
                                global_status.channels_energy_analysis[id].obj = dl_energy;
                            } else {
                                if (verbosity > 0)
                                {
                                    char time_buffer[BUFFER_SIZE];
                                    time_string(time_buffer, BUFFER_SIZE, NULL);
                                    std::cout << '[' << time_buffer << "] ";
                                    std::cout << "ERROR: unable to load the functions; ";
                                    std::cout << std::endl;
                                }
                            }
                        }
                    }
                }

                ////////////////////////////////////////////////////////////////
                // Libraries storing in global_status                         //
                ////////////////////////////////////////////////////////////////
                if (!dl_loading_error) {
                    void *timestamp_user_config = NULL;
                    void *energy_user_config = NULL;

                    json_t *user_config = json_object_get(value, "user_config");
            
                    if (!json_is_object(user_config)) {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "WARNING: user_config is not an object; ";
                        std::cout << std::endl;
                    } else {
                        if (verbosity > 0)
                        {
                            char time_buffer[BUFFER_SIZE];
                            time_string(time_buffer, BUFFER_SIZE, NULL);
                            std::cout << '[' << time_buffer << "] ";
                            std::cout << "Calling user init; ";
                            std::cout << "Channel: " << id << "; ";
                            std::cout << std::endl;
                        }

                        global_status.channels_timestamp_init[id].fn(user_config,
                                                                     &timestamp_user_config);
                        global_status.channels_energy_init[id].fn(user_config,
                                                                  &energy_user_config);
                    }

                    global_status.channels_timestamp_user_config[id] = timestamp_user_config;
                    global_status.channels_energy_user_config[id] = energy_user_config;
                    global_status.partial_counts[id] = 0;

                    global_status.active_channels.insert(id);
                }
            }
        }
    }

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Configuration of waan completed successfully!";
        std::cout << std::endl;
    }

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

    void *data_output_socket = zmq_socket(context, ZMQ_PUB);
    if (!data_output_socket)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on output data socket creation: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }
    void *data_input_socket = zmq_socket(context, ZMQ_SUB);
    if (!data_input_socket)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on input data socket creation: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

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
    global_status.data_input_socket = data_input_socket;
    global_status.data_output_socket = data_output_socket;
    global_status.commands_socket = commands_socket;

    std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));

    return states::BIND_SOCKETS;
}

state actions::bind_sockets(status &global_status)
{
    std::string status_address = global_status.status_address;
    std::string data_output_address = global_status.data_output_address;
    std::string data_input_address = global_status.data_input_address;
    std::string commands_address = global_status.commands_address;

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

    const int d = zmq_bind(global_status.data_output_socket, data_output_address.c_str());
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

    const int a = zmq_connect(global_status.data_input_socket, data_input_address.c_str());
    if (a != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on data socket binding: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

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

    zmq_setsockopt(global_status.data_input_socket,
                   ZMQ_SUBSCRIBE,
                   defaults_abcd_data_waveforms_topic,
                   strlen(defaults_abcd_data_waveforms_topic));

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

    if (json_is_object(global_status.config)) {
    	json_decref(global_status.config);
    }

    global_status.config = new_config;

    return states::APPLY_CONFIG;
}

state actions::apply_config(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string("Configuration"));

    actions::generic::publish_message(global_status, defaults_waan_events_topic, json_event_message);

    json_decref(json_event_message);

    const bool success = actions::generic::configure(global_status);

    if (success)
    {
        return states::PUBLISH_STATUS;
    }
    else
    {
        return states::CONFIGURE_ERROR;
    }
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
    if (status_message == NULL)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to create status_message json; ";
        std::cout << std::endl;

        // I am not sure what to do here...
        //return false;
    }

    json_t *active_channels = json_array();
    if (active_channels == NULL)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to create active_channels json; ";
        std::cout << std::endl;

        json_decref(status_message);

        // I am not sure what to do here...
        //return false;
    }

    json_t *disabled_channels = json_array();
    if (disabled_channels == NULL)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to create disabled_channels json; ";
        std::cout << std::endl;

        json_decref(status_message);
        json_decref(active_channels);

        // I am not sure what to do here...
        //return false;
    }

    json_t *channels_statuses = json_array();
    if (channels_statuses == NULL)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to create active_channels json; ";
        std::cout << std::endl;

        json_decref(status_message);
        json_decref(active_channels);
        json_decref(disabled_channels);

        // I am not sure what to do here...
        //return false;
    }

    const auto now = std::chrono::system_clock::now();
    const auto pub_delta_time = std::chrono::duration_cast<std::chrono::duration<long int>>(now - global_status.last_publication);
    const double pubtime = static_cast<double>(pub_delta_time.count());

    for (const unsigned int &channel: global_status.active_channels)
    {
        const unsigned int channel_counts = global_status.partial_counts[channel];
        const double channel_rate = channel_counts / pubtime;

        global_status.partial_counts[channel] = 0;

        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Publishing status for active channel: " << channel << "; ";
            std::cout << std::endl;
        }

        json_t *channel_status = json_object();

        json_object_set_new_nocheck(channel_status, "id", json_integer(channel));
        json_object_set_new_nocheck(channel_status, "enabled", json_true());
        json_object_set_new_nocheck(channel_status, "rate", json_real(channel_rate));
        json_array_append_new(channels_statuses, channel_status);

        json_array_append_new(active_channels, json_integer(channel));
    }

    for (const unsigned int &channel: global_status.disabled_channels)
    {
        json_array_append_new(disabled_channels, json_integer(channel));
    }
    json_object_set_new_nocheck(status_message, "statuses", channels_statuses);
    json_object_set_new_nocheck(status_message, "active_channels", active_channels);
    json_object_set_new_nocheck(status_message, "disabled_channels", disabled_channels);
    json_object_set_new_nocheck(status_message, "config", json_deep_copy(global_status.config));

    actions::generic::publish_message(global_status, defaults_waan_status_topic, status_message);

    json_decref(status_message);

    const std::chrono::time_point<std::chrono::system_clock> last_publication = std::chrono::system_clock::now();
    global_status.last_publication = last_publication;

    return states::RECEIVE_COMMANDS;
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

        if (json_command != NULL && json_is_string(json_command))
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

            if (command == std::string("reconfigure") && json_arguments) {
                json_t *new_config = json_object_get(json_arguments, "config");

                // Remember to clean up
                json_decref(global_status.config);

                // This is a borrowed reference, so we shall increment the reference count
                global_status.config = new_config;
                json_incref(global_status.config);

                json_t *json_event_message = json_object();

                json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
                json_object_set_new_nocheck(json_event_message, "event", json_string("Reconfiguration"));

                actions::generic::publish_message(global_status, defaults_waan_events_topic, json_event_message);

                json_decref(json_event_message);

                return states::APPLY_CONFIG;
            } else if (command == std::string("off")) {
                return states::CLOSE_SOCKETS;
            } else if (command == std::string("quit")) {
                return states::CLOSE_SOCKETS;
            }
        }
    }

    json_decref(json_message);

    return states::READ_SOCKET;
}

state actions::read_socket(status &global_status)
{
    void *data_input_socket = global_status.data_input_socket;
    const int verbosity = global_status.verbosity;

    char *topic = NULL;
    char *input_buffer = NULL;
    size_t size;
    size_t inner_counter = 0;

    int result = receive_byte_message(data_input_socket, &topic, (void **)(&input_buffer), &size, true, global_status.verbosity);

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    const auto publish_period = std::chrono::seconds(global_status.publish_period);

    while (size > 0
           && result == EXIT_SUCCESS
           && (now - global_status.last_publication < publish_period))
    {
        inner_counter += 1;

        const std::string topic_string(topic);

        if (verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Message size: " << size << "; ";
            std::cout << "Topic: " << topic << "; ";
            std::cout << "Inner counter: " << inner_counter << "; ";
            std::cout << "verbosity: " << verbosity << "; ";
            std::cout << "Topic search: " << topic_string.find(defaults_abcd_data_waveforms_topic) << "; ";
            std::cout << std::endl;
        }

        if (topic_string.find(defaults_abcd_data_waveforms_topic) == 0) {
            if (verbosity > 1)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Waveform message to be analyzed; ";
                std::cout << std::endl;
            }

            const clock_t event_start = clock();
            size_t events_number = 0;

            std::vector<struct event_PSD> output_events;
            std::vector<uint8_t> output_waveforms;

            // We reserve the memory for the output events using a big enough
            // number, to reduce it we arbitrarily divide it by the event size.
            output_events.reserve(size / sizeof(struct event_PSD));
            // We reserve the memory for the output buffer twice as big as the
            // input buffer that is most likely to be big enough for the output
            // waveforms data as the gates are half as big as the samples.
            output_waveforms.reserve(size * defaults_waan_waveforms_buffer_multiplier);

            size_t input_offset = 0;

            // We add 14 bytes to the input_offset because we want to be sure to read at least
            // the first header of the waveform
            while ((input_offset + 14) < size)
            {
                if (verbosity > 1)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Message size: " << (unsigned long)size << "; ";
                    std::cout << "Input offset: " << (unsigned long)input_offset << "; ";
                    std::cout << std::endl;
                }

                uint64_t timestamp = *((uint64_t *)(input_buffer + input_offset));
                const uint8_t this_channel = *((uint8_t *)(input_buffer + input_offset + 8));
                const uint32_t samples_number = *((uint32_t *)(input_buffer + input_offset + 9));
                const uint8_t gates_number = *((uint8_t *)(input_buffer + input_offset + 13));

                if (verbosity > 1)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Channel: " << (unsigned int)this_channel << "; ";
                    std::cout << "Samples number: " << (unsigned int)samples_number << "; ";
                    std::cout << std::endl;
                }

                const bool is_active = std::find(global_status.active_channels.begin(),
                                                 global_status.active_channels.end(),
                                                 this_channel) != global_status.active_channels.end();
                const size_t needed_offset = input_offset + 14
                                           + (samples_number * sizeof(uint16_t))
                                           + (samples_number * gates_number * sizeof(uint8_t));

                if (!is_active) {
                    if (verbosity > 0)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "Channel " << (unsigned int)this_channel << " is disabled; ";
                        std::cout << std::endl;
                    }

                    global_status.disabled_channels.insert(this_channel);
                }

                if  (needed_offset > size) {
                    if (verbosity > 0)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "WARNING: Uncomplete waveform in buffer; ";
                        std::cout << "needed offset: " << needed_offset << "; size:" << size << "; ";
                        std::cout << std::endl;
                    }
                }

                if (is_active && (needed_offset <= size)) {

                    if (verbosity > 1)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "Channel is active, reading samples; ";
                        std::cout << std::endl;
                    }

                    events_number += 1;

                    const uint16_t *samples = (uint16_t *)(input_buffer + input_offset + 14);
                    // Should I store the digitizer gates to the waveform?
                    // They are not standard and not quantitative, let's not bother
                    //const uint8_t *gates = (uint8_t *)(input_buffer + input_offset + 14 + samples_number * sizeof(uint16_t));

                    struct event_PSD this_event = {timestamp, 0, 0, 0, this_channel, 0};
                    struct event_waveform this_waveform = waveform_create(timestamp,
                                                                          this_channel,
                                                                          samples_number,
                                                                          0);

                    waveform_samples_set(&this_waveform, samples);

                    //for (uint8_t i = 0; i < gates_number; i++) {
                    //    waveform_additional_set(&this_waveform,
                    //                            i,
                    //                            gates + samples_number * i * sizeof(uint8_t));
                    //}

                    uint32_t trigger_position = 0;
                    int8_t timestamp_is_selected = SELECT_FALSE;
                    int8_t energy_is_selected = SELECT_FALSE;

                    if (verbosity > 1)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "Timestamp analysis; ";
                        std::cout << std::endl;
                    }

                    global_status.channels_timestamp_analysis[this_channel].
                        fn(samples,
                           samples_number,
                           &trigger_position,
                           &this_waveform,
                           &this_event,
                           &timestamp_is_selected,
                           global_status.channels_timestamp_user_config[this_channel]);

                    if (verbosity > 1)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "Trigger position: " << (unsigned long)trigger_position << "; ";
                        std::cout << "Samples number: " << (unsigned int)samples_number << "; ";
                        std::cout << "selected: " << ((timestamp_is_selected > 0) ? "true" : "false") << "; ";
                        std::cout << std::endl;
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "Energy analysis; ";
                        std::cout << std::endl;
                    }

                    global_status.channels_energy_analysis[this_channel].
                        fn(samples,
                           samples_number,
                           trigger_position,
                           &this_waveform,
                           &this_event,
                           &energy_is_selected,
                           global_status.channels_energy_user_config[this_channel]);

                    if (timestamp_is_selected && energy_is_selected && global_status.forward_waveforms) {
                        if (!global_status.enable_additional) {
                            waveform_additional_set_number(&this_waveform, 0);
                        }

                        const size_t current_waveform_buffer_size = output_waveforms.size();
                        const size_t this_waveform_size = waveform_size(&this_waveform);

                        output_waveforms.resize(current_waveform_buffer_size + this_waveform_size);

                        memcpy(output_waveforms.data() + current_waveform_buffer_size,
                               reinterpret_cast<void*>(waveform_serialize(&this_waveform)),
                               this_waveform_size);
                    }

                    if (timestamp_is_selected && energy_is_selected) {
                        global_status.partial_counts[this_channel] += 1;

                        output_events.push_back(this_event);
                    }

                    waveform_destroy_samples(&this_waveform);
                }

                // Compute the waveform event size
                const size_t this_size = 14 + samples_number * sizeof(uint16_t) + gates_number * samples_number * sizeof(uint8_t);
                input_offset += this_size;
            }

            const size_t total_waveforms_size = output_waveforms.size();

            if (total_waveforms_size > 0) {
                std::string topic = defaults_abcd_data_waveforms_topic;
                topic += "_v0";
                topic += "_n";
                topic += std::to_string(global_status.waveforms_msg_ID);
                topic += "_s";
                topic += std::to_string(total_waveforms_size);

                if (global_status.verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Sending waveforms buffer; ";
                    std::cout << "Topic: " << topic << "; ";
                    std::cout << "buffer size: " << total_waveforms_size << "; ";
                    std::cout << std::endl;
                }

                const int result = send_byte_message(global_status.data_output_socket,
                                                     topic.c_str(),
                                                     reinterpret_cast<void*>(output_waveforms.data()),
                                                     total_waveforms_size, 1);

                global_status.waveforms_msg_ID += 1;

                if (result == EXIT_FAILURE)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ZeroMQ Error publishing events waveforms";
                    std::cout << std::endl;
                }
            }

            const size_t total_events_size = output_events.size() * sizeof(struct event_PSD);

            if (total_events_size > 0) {
                std::string topic = defaults_abcd_data_events_topic;
                topic += "_v0";
                topic += "_n";
                topic += std::to_string(global_status.events_msg_ID);
                topic += "_s";
                topic += std::to_string(total_events_size);

                if (global_status.verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Sending events buffer; ";
                    std::cout << "Topic: " << topic << "; ";
                    std::cout << "buffer size: " << total_events_size << "; ";
                    std::cout << std::endl;
                }

                const int result = send_byte_message(global_status.data_output_socket,
                                                     topic.c_str(),
                                                     reinterpret_cast<void*>(output_events.data()),
                                                     total_events_size, 1);

                global_status.events_msg_ID += 1;

                if (result == EXIT_FAILURE)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ZeroMQ Error publishing events PSDs";
                    std::cout << std::endl;
                }
            }

            const clock_t event_stop = clock();

            if (verbosity > 0)
            {
                const float elaboration_time = (float)(event_stop - event_start) / CLOCKS_PER_SEC * 1000;
                const float elaboration_speed = size / elaboration_time * 1000.0 / 1024.0 / 1024.0;
                const float elaboration_rate = events_number / elaboration_time * 1000.0;
                            
                printf("size: %zu; events_number: %zu; elaboration_time: %f ms; elaboration_speed: %f MBi/s, %f evts/s\n", size, events_number, elaboration_time, elaboration_speed, elaboration_rate);
            }
        }

        free(topic);
        free(input_buffer);

        topic = NULL;
        input_buffer = NULL;

        result = receive_byte_message(data_input_socket, &topic, (void **)(&input_buffer), &size, true, global_status.verbosity);
    }

    if (topic) {
        free(topic);
    }
    if (input_buffer) {
        free(input_buffer);
    }

    if (now - global_status.last_publication > publish_period)
    {
        return states::PUBLISH_STATUS;
    }

    return states::READ_SOCKET;
}

state actions::clear_memory(status &global_status)
{
    actions::generic::clear_memory(global_status);

    if (json_is_object(global_status.config)) {
    	json_decref(global_status.config);

        global_status.config = NULL;
    }

    return states::CLOSE_SOCKETS;
}

state actions::close_sockets(status &global_status)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));

    void *status_socket = global_status.status_socket;
    void *data_input_socket = global_status.data_input_socket;
    void *data_output_socket = global_status.data_output_socket;
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

    const int i = zmq_close(data_input_socket);
    if (i != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on input data socket close: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    const int o = zmq_close(data_output_socket);
    if (o != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on output data socket close: ";
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

state actions::stop(status&)
{
    return states::STOP;
}

state actions::communication_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Communication error"));

    actions::generic::publish_message(global_status, defaults_waan_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::CLOSE_SOCKETS;
}

state actions::parse_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Config parse error"));

    actions::generic::publish_message(global_status, defaults_waan_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::PUBLISH_STATUS;
}

state actions::configure_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Configure error"));

    actions::generic::publish_message(global_status, defaults_waan_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::PUBLISH_STATUS;
}
