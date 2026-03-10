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

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

extern "C" {
#include <zmq.h>
#include <jansson.h>
// For PRIu8 and PRIu32
#include <inttypes.h>
}

#include "typedefs.hpp"
#include "states.hpp"
#include "actions.hpp"

extern "C" {
#include "defaults.h"
#include "utilities_functions.h"
#include "files_functions.h"
#include "socket_functions.h"
#include "jansson_socket_functions.h"
#include "events.h"
#include "analysis_functions.h"
}

/******************************************************************************/
/* Generic actions                                                            */
/******************************************************************************/

void actions::generic::publish_message(status &global_status,
                                       std::string topic,
                                       json_t *status_message)
{
    void *status_socket = global_status.status_socket;
    const unsigned long int status_msg_ID = global_status.status_msg_ID;

    char time_buffer[std::size("  yyyy-mm-ddThh:mm:ssZ+00:00  ")];
    time_string(time_buffer, std::size(time_buffer), NULL);

    json_object_set_new(status_message, "module", json_string("waan"));
    json_object_set_new(status_message, "timestamp", json_string(time_buffer));
    json_object_set_new(status_message, "msg_ID", json_integer(status_msg_ID));

    send_json_message(status_socket, const_cast<char*>(topic.c_str()), status_message, 0);

    global_status.status_msg_ID += 1;
}

void actions::generic::clear_memory(status &global_status)
{
    global_status.logger_console->info("Cleaning up memory");

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

        global_status.logger_console->info("Calling user close on the timestamp_user_config; Channel: {}", id);

        global_status.channels_timestamp_close[id].fn(timestamp_config);
    }
    global_status.channels_timestamp_user_config.clear();

    global_status.channels_timestamp_close.clear();

    for (auto& pair : global_status.channels_energy_user_config) {
        const unsigned int id = pair.first;
        void *const energy_config = pair.second;

        global_status.logger_console->info("Calling user close on the energy_user_config; Channel: {}", id);

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
    global_status.logger_console->info("Configuring waan");

    actions::generic::clear_memory(global_status);

    json_t *config = global_status.config;

    ////////////////////////////////////////////////////////////////////////////
    // Starting the global configuration                                      //
    ////////////////////////////////////////////////////////////////////////////
    global_status.logger_console->info("Global configuration");

    global_status.forward_waveforms = json_is_true(json_object_get(config, "forward_waveforms"));
    global_status.enable_additional = json_is_true(json_object_get(config, "enable_additional"));

    if (json_is_string(json_object_get(config, "high_water_mark"))) {
        const char *high_water_mark_str = json_string_value(json_object_get(config, "high_water_mark"));
        const std::string high_water_mark = high_water_mark_str ? high_water_mark_str : "";

        if (high_water_mark == std::string("no limit")) {
            // According to the ZeroMQ documentation a zero means no limit.
            global_status.high_water_mark = 0;
        } else {
            // FIXME: What should the default value be? No limit? Then it's the previous case
            global_status.high_water_mark = 0;
        }
    } else {
        global_status.high_water_mark = json_number_value(json_object_get(config, "high_water_mark"));
    }

    if (global_status.data_input_source == SOCKET_INPUT) {
        zmq_setsockopt(global_status.data_input_socket,
                       ZMQ_RCVHWM,
                       &global_status.high_water_mark,
                       sizeof(global_status.high_water_mark));
    }

    if (global_status.high_water_mark == 0) {
        json_object_set(config, "high_water_mark", json_string("no limit"));
    } else {
        json_object_set(config, "high_water_mark", json_integer(global_status.high_water_mark));
    }

    global_status.logger_console->info("Forward waveforms: {}", global_status.forward_waveforms);
    global_status.logger_console->info("Enable additional: {}", global_status.enable_additional);
    global_status.logger_console->info("High water mark: {}", global_status.high_water_mark);

    ////////////////////////////////////////////////////////////////////////////
    // Starting the single channels configuration                             //
    ////////////////////////////////////////////////////////////////////////////
    global_status.logger_console->info("Channels configuration");

    json_t *json_channels = json_object_get(config, "channels");

    if (json_channels == NULL || !json_is_array(json_channels))
    {
        global_status.logger_error->warn("No channels configuration");

        json_channels = json_array();

        json_object_set_nocheck(config, "channels", json_channels);
    }

    size_t index;
    json_t *value;

    json_array_foreach(json_channels, index, value) {
        // The id may be a single integer or an array of integers
        json_t *json_id = json_object_get(value, "id");

        std::vector<int> channel_ids;

        if (json_id != NULL && json_is_integer(json_id)) {
            const int id = json_number_value(json_id);
            channel_ids.push_back(id);
        } else if (json_id != NULL && json_is_array(json_id)) {
            size_t id_index;
            json_t *id_value;

            json_array_foreach(json_id, id_index, id_value) {
                if (id_value != NULL && json_is_integer(id_value)) {
                    const int id = json_number_value(id_value);
                    channel_ids.push_back(id);
                }
            }
        }

        if (channel_ids.size() > 0) {
            global_status.logger_console->info("Found channel(s): {}", fmt::join(channel_ids, ", "));

            const bool enabled = json_is_true(json_object_get(value, "enable"))
                                 || json_is_true(json_object_get(value, "enabled"));

            global_status.logger_console->info("Channel is {}", enabled ? "enabled" : "disabled");

            bool dl_loading_error = false;

            json_t *libraries_json = json_object_get(value, "user_libraries");

            ////////////////////////////////////////////////////////////////
            // Libraries loading                                          //
            ////////////////////////////////////////////////////////////////
            if (!enabled) {
                global_status.logger_console->info("Not loading libraries for disabled channel");

                dl_loading_error = true;
            } else if (!json_is_object(libraries_json)) {
                global_status.logger_error->error("user_libraries is not an object");

                dl_loading_error = true;
            } else {
                const char *json_timestamp =
                    json_string_value(json_object_get(libraries_json, "timestamp"));
                const char *json_energy =
                    json_string_value(json_object_get(libraries_json, "energy"));

                const std::string lib_timestamp =
                    json_timestamp ? json_timestamp : "";
                const std::string lib_energy =
                    json_energy ? json_energy : "";

                global_status.logger_console->info("Library for timestamp: {}", lib_timestamp);
                global_status.logger_console->info("Library for energy: {}", lib_energy);

                ////////////////////////////////////////////////////////////
                // Timestamp analysis library                             //
                ////////////////////////////////////////////////////////////
                if (lib_timestamp.length() == 0) {
                    global_status.logger_error->warn("Empty timestamp library, using dummy_timestamp_analysis()");

                    for (auto& id : channel_ids) {
                        global_status.channels_timestamp_init[id].fn = dummy_init;
                        global_status.channels_timestamp_close[id].fn = dummy_close;
                        global_status.channels_timestamp_analysis[id].fn = dummy_timestamp_analysis;
                    }
                } else {
                    global_status.logger_console->info("Loading library: {}", lib_timestamp);

                    void *dl_handle = dlopen(lib_timestamp.c_str(), RTLD_NOW);

                    if (!dl_handle) {
                        const std::string error_description = dlerror();

                        global_status.logger_error->error("Unable to load timestamp library: {}", error_description);

                        for (auto& id : channel_ids) {
                            const std::string event_description = "Load error on channel: " + std::to_string(id) \
                                                                  + "; Unable to load library: " + lib_timestamp \
                                                                  + " (" + error_description + ")";

                            json_t *json_event_message = json_object();

                            json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
                            json_object_set_new_nocheck(json_event_message, "error", json_string(event_description.c_str()));

                            actions::generic::publish_message(global_status, defaults_waan_events_topic, json_event_message);

                            json_decref(json_event_message);
                        }

                        dl_loading_error = true;
                    } else {
                        global_status.logger_console->info("Loading timestamp_init() function");

                        void *dl_init = dlsym(dl_handle, "timestamp_init");

                        if (!dl_init) {
                            global_status.logger_error->warn("Unable to load timestamp_init() function: {}", dlerror());

                            for (auto& id : channel_ids) {
                                global_status.channels_timestamp_init[id].fn = dummy_init;
                            }
                        } else {
                            for (auto& id : channel_ids) {
                                global_status.channels_timestamp_init[id].obj = dl_init;
                            }
                        }

                        global_status.logger_console->info("Loading timestamp_close() function");

                        void *dl_close = dlsym(dl_handle, "timestamp_close");

                        if (!dl_close) {
                            global_status.logger_error->warn("Unable to load timestamp_close() function: {}", dlerror());

                            for (auto& id : channel_ids) {
                                global_status.channels_timestamp_close[id].fn = dummy_close;
                            }
                        } else {
                            for (auto& id : channel_ids) {
                                global_status.channels_timestamp_close[id].obj = dl_close;
                            }
                        }

                        global_status.logger_console->info("Loading timestamp_analysis() function");

                        void *dl_timestamp = dlsym(dl_handle, "timestamp_analysis");

                        if (!dl_timestamp) {
                            const std::string error_description = dlerror();

                            global_status.logger_error->error("Unable to load timestamp_analysis() function: {}", error_description);

                            for (auto& id : channel_ids) {
                                const std::string event_description = "Load error on channel: " + std::to_string(id) \
                                                                      + "; Unable to load the timestamp_analysis() function from library: " + lib_timestamp \
                                                                      + " (" + error_description + ")";

                                json_t *json_event_message = json_object();

                                json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
                                json_object_set_new_nocheck(json_event_message, "error", json_string(event_description.c_str()));

                                actions::generic::publish_message(global_status, defaults_waan_events_topic, json_event_message);

                                json_decref(json_event_message);
                            }

                            dl_loading_error = true;
                        }

                        if (!dl_loading_error) {
                            global_status.logger_console->info("Successfully loaded the functions");

                            for (auto& id : channel_ids) {
                                global_status.dl_timestamp_handles[id] = dl_handle;
                                global_status.channels_timestamp_analysis[id].obj = dl_timestamp;
                            }
                        } else {
                            global_status.logger_error->error("Unable to load the functions");
                        }
                    }
                }

                ////////////////////////////////////////////////////////////
                // Energy analysis library                                //
                ////////////////////////////////////////////////////////////
                if (lib_energy.length() == 0) {
                    global_status.logger_error->error("Empty energy library");

                    dl_loading_error = true;
                } else {
                    global_status.logger_console->info("Loading library: {}", lib_energy);

                    void *dl_handle = dlopen(lib_energy.c_str(), RTLD_NOW);

                    if (!dl_handle) {
                        const std::string error_description = dlerror();

                        global_status.logger_error->error("Unable to load energy library: {}", error_description);

                        for (auto& id : channel_ids) {
                            const std::string event_description = "Load error on channel: " + std::to_string(id) \
                                                                  + "; Unable to load library: " + lib_energy \
                                                                  + " (" + error_description + ")";

                            json_t *json_event_message = json_object();

                            json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
                            json_object_set_new_nocheck(json_event_message, "error", json_string(event_description.c_str()));

                            actions::generic::publish_message(global_status, defaults_waan_events_topic, json_event_message);

                            json_decref(json_event_message);
                        }

                        dl_loading_error = true;
                    } else {
                        global_status.logger_console->info("Loading energy_init() function");

                        void *dl_init = dlsym(dl_handle, "energy_init");

                        if (!dl_init) {
                            global_status.logger_error->warn("Unable to load energy_init() function: {}", dlerror());

                            for (auto& id : channel_ids) {
                                global_status.channels_energy_init[id].fn = dummy_init;
                            }
                        } else {
                            for (auto& id : channel_ids) {
                                global_status.channels_energy_init[id].obj = dl_init;
                            }
                        }

                        global_status.logger_console->info("Loading energy_close() function");

                        void *dl_close = dlsym(dl_handle, "energy_close");

                        if (!dl_close) {
                            global_status.logger_error->warn("Unable to load energy_close() function: {}", dlerror());

                            for (auto& id : channel_ids) {
                                global_status.channels_energy_close[id].fn = dummy_close;
                            }
                        } else {
                            for (auto& id : channel_ids) {
                                global_status.channels_energy_close[id].obj = dl_close;
                            }
                        }

                        global_status.logger_console->info("Loading energy_analysis() function");

                        void *dl_energy = dlsym(dl_handle, "energy_analysis");

                        if (!dl_energy) {
                            const std::string error_description = dlerror();

                            global_status.logger_error->error("Unable to load energy_analysis() function: {}", error_description);

                            for (auto& id : channel_ids) {
                                const std::string event_description = "Load error on channel: " + std::to_string(id) \
                                                                      + "; Unable to load the energy_analysis() function from library: " + lib_energy \
                                                                      + " (" + error_description + ")";

                                json_t *json_event_message = json_object();

                                json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
                                json_object_set_new_nocheck(json_event_message, "error", json_string(event_description.c_str()));

                                actions::generic::publish_message(global_status, defaults_waan_events_topic, json_event_message);

                                json_decref(json_event_message);
                            }

                            dl_loading_error = true;
                        }

                        if (!dl_loading_error) {
                            global_status.logger_console->info("Successfully loaded the functions");

                            for (auto& id : channel_ids) {
                                global_status.dl_energy_handles[id] = dl_handle;
                                global_status.channels_energy_analysis[id].obj = dl_energy;
                            }
                        } else {
                            global_status.logger_error->error("Unable to load the functions");
                        }
                    }
                }
            }

            ////////////////////////////////////////////////////////////////
            // Libraries storing in global_status                         //
            ////////////////////////////////////////////////////////////////
            if (!dl_loading_error) {
                for (auto& id : channel_ids) {
                    void *timestamp_user_config = NULL;
                    void *energy_user_config = NULL;

                    json_t *user_config = json_object_get(value, "user_config");

                    if (user_config == NULL || !json_is_object(user_config)) {
                        global_status.logger_error->warn("user_config is not an object for channel {}", id);

                        user_config = json_object();

                        json_object_set_nocheck(value, "user_config", user_config);
                    }

                    global_status.logger_console->info("Calling user inits; Channel: {}", id);

                    global_status.channels_timestamp_init[id].fn(user_config,
                                                                 &timestamp_user_config);
                    global_status.channels_energy_init[id].fn(user_config,
                                                              &energy_user_config);

                    global_status.channels_timestamp_user_config[id] = timestamp_user_config;
                    global_status.channels_energy_user_config[id] = energy_user_config;
                    global_status.partial_counts[id] = 0;

                    global_status.active_channels.insert(id);
                }
            }
        }
    }

    global_status.logger_console->info("Configuration of waan completed successfully!");

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
        global_status.logger_error->error("ZeroMQ Error on context creation");

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
        global_status.logger_error->error("ZeroMQ Error on status socket creation: {}", zmq_strerror(errno));

        return states::COMMUNICATION_ERROR;
    }

    void *data_output_socket = zmq_socket(context, ZMQ_PUB);
    if (!data_output_socket)
    {
        global_status.logger_error->error("ZeroMQ Error on data output socket creation: {}", zmq_strerror(errno));

        return states::COMMUNICATION_ERROR;
    }
    // Creating a data_input_socket even if we are going to read data from a
    // file, it should not hurt and it makes things easier
    // It will not be connected afterwards
    void *data_input_socket = zmq_socket(context, ZMQ_SUB);
    if (!data_input_socket)
    {
        global_status.logger_error->error("ZeroMQ Error on data input socket creation: {}", zmq_strerror(errno));

        return states::COMMUNICATION_ERROR;
    }

    void *commands_socket = zmq_socket(context, ZMQ_PULL);
    if (!commands_socket)
    {
        global_status.logger_error->error("ZeroMQ Error on commands socket creation: {}", zmq_strerror(errno));

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
        global_status.logger_error->error("ZeroMQ Error on status socket binding: {}", zmq_strerror(errno));

        return states::COMMUNICATION_ERROR;
    }

    const int d = zmq_bind(global_status.data_output_socket, data_output_address.c_str());
    if (d != 0)
    {
        global_status.logger_error->error("ZeroMQ Error on data output socket binding: {}", zmq_strerror(errno));

        return states::COMMUNICATION_ERROR;
    }

    global_status.data_input_source = get_input_source_type(data_input_address.c_str());

    if (global_status.data_input_source == RAW_FILE_INPUT) {
        char *data_input_filename = NULL;
        size_t ade_buffer_size = 0;

        get_filename_and_buffersize(data_input_address.c_str(), &data_input_filename, &ade_buffer_size);

        global_status.logger_console->info("Opening data file: {}", data_input_filename);

        global_status.data_input_file = fopen(data_input_filename, "rb");

        if (!global_status.data_input_file) {
            global_status.logger_error->error("Unable to open file {} to read", data_input_filename);

            return states::COMMUNICATION_ERROR;
        }
    } else {
        global_status.logger_console->info("Connecting data input socket to: {}", data_input_address);

        const int a = zmq_connect(global_status.data_input_socket, data_input_address.c_str());
        if (a != 0)
        {
            global_status.logger_error->error("ZeroMQ Error on data input socket connection: {}", zmq_strerror(errno));

            return states::COMMUNICATION_ERROR;
        }

        zmq_setsockopt(global_status.data_input_socket,
                       ZMQ_SUBSCRIBE,
                       defaults_abcd_data_waveforms_topic,
                       strlen(defaults_abcd_data_waveforms_topic));
    }

    const int c = zmq_bind(global_status.commands_socket, commands_address.c_str());
    if (c != 0)
    {
        global_status.logger_error->error("ZeroMQ Error on commands socket binding: {}", zmq_strerror(errno));

        return states::COMMUNICATION_ERROR;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));

    // Wait a bit to prevent the slow-joiner syndrome
    std::this_thread::sleep_for(std::chrono::milliseconds(defaults_all_slow_joiner_wait));

    return states::READ_CONFIG;
}

state actions::read_config(status &global_status)
{
    const std::string config_filename = global_status.config_filename;

    global_status.logger_console->info("Reading config file: {}", config_filename);

    json_error_t error;

    json_t *new_config = json_load_file(config_filename.c_str(), 0, &error);

    if (!new_config)
    {
        global_status.logger_error->error("Parse error while reading config file: {} (source: {}, line: {}, column: {}, position: {}", error.text, error.source, error.line, error.column, error.position);

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
    global_status.logger_console->info("Publishing status");

    json_t *status_message = json_object();
    if (status_message == NULL)
    {
        global_status.logger_error->error("Unable to create status_message json");

        // I am not sure what to do here...
        //return false;
    }

    json_t *active_channels = json_array();
    if (active_channels == NULL)
    {
        global_status.logger_error->error("Unable to create active_channels json");

        json_decref(status_message);

        // I am not sure what to do here...
        //return false;
    }

    json_t *disabled_channels = json_array();
    if (disabled_channels == NULL)
    {
        global_status.logger_error->error("Unable to create disabled_channels json");

        json_decref(status_message);
        json_decref(active_channels);

        // I am not sure what to do here...
        //return false;
    }

    json_t *channels_statuses = json_array();
    if (channels_statuses == NULL)
    {
        global_status.logger_error->error("Unable to create active_channels json");

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

        global_status.logger_console->info("Publishing status for active channel: {}", channel);

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
    json_object_set_new_nocheck(status_message, "config_file", json_string(global_status.config_filename.c_str()));

    actions::generic::publish_message(global_status, defaults_waan_status_topic, status_message);

    json_decref(status_message);

    const std::chrono::time_point<std::chrono::system_clock> last_publication = std::chrono::system_clock::now();
    global_status.last_publication = last_publication;

    return states::RECEIVE_COMMANDS;
}

state actions::receive_commands(status &global_status)
{
    void *commands_socket = global_status.commands_socket;

    global_status.logger_console->debug("Receiving command...");

    json_t *json_message = NULL;

    const int result = receive_json_message(commands_socket, NULL, &json_message, false, 0);

    if (!json_message || result == EXIT_FAILURE)
    {
        global_status.logger_error->error("Error on receiving JSON commands message");
    }
    else
    {
        const size_t command_ID = json_integer_value(json_object_get(json_message, "msg_ID"));

        global_status.logger_console->debug("Received command; msg_ID: {}", command_ID);

        json_t *json_command = json_object_get(json_message, "command");
        json_t *json_arguments = json_object_get(json_message, "arguments");

        if (json_command != NULL && json_is_string(json_command))
        {
            const std::string command = json_string_value(json_command);

            global_status.logger_console->debug("Received command: {}", command);

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
            } else if (command == std::string("store_configuration")) {
                const std::string config_filename = global_status.config_filename;

                const int r = json_dump_file(global_status.config, config_filename.c_str(), JSON_INDENT(4));

                if (r < 0)
                {
                    std::string event_description = "Unable to store configuration file to: " + config_filename;

                    global_status.logger_error->error(event_description);

                    json_t *json_event_message = json_object();

                    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
                    json_object_set_new_nocheck(json_event_message, "error", json_string(event_description.c_str()));

                    actions::generic::publish_message(global_status, defaults_waan_events_topic, json_event_message);

                    json_decref(json_event_message);

                } else {
                    const std::string event_description = "Stored configuration to " + config_filename;

                    json_t *json_event_message = json_object();

                    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
                    json_object_set_new_nocheck(json_event_message, "event", json_string(event_description.c_str()));

                    actions::generic::publish_message(global_status, defaults_waan_events_topic, json_event_message);

                    json_decref(json_event_message);
                }
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
    FILE *data_input_file = global_status.data_input_file;

    char *topic = NULL;
    char *buffer_input = NULL;
    size_t size;
    int result = EXIT_FAILURE;
    // Using int here instead of size_t to be compatible with the
    // high_water_mark, that for the ZeroMQ must be an int.
    int inner_counter = 0;

    if (global_status.data_input_source == RAW_FILE_INPUT) {
        result = read_byte_message_from_adr(data_input_file, &topic, (void **)(&buffer_input), &size, true, 0);
    } else {
        result = receive_byte_message(data_input_socket, &topic, (void **)(&buffer_input), &size, true, 0);
    }

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    const auto publish_period = std::chrono::seconds(global_status.publish_period);

    while (size > 0
           && result == EXIT_SUCCESS
           && (now - global_status.last_publication < publish_period))
    {
        inner_counter += 1;

        const std::string topic_string(topic);

        global_status.logger_console->debug("Message size: {}; Topic: {}; inner_counter: {}; topic search: {}", size, inner_counter, topic_string.find(defaults_abcd_data_waveforms_topic));

        if (topic_string.find(defaults_abcd_data_waveforms_topic) == 0) {

            // According to the ZeroMQ documentation a high_water_mark of zero
            // means no limit, so we use the same convention.
            if (inner_counter > global_status.high_water_mark && global_status.high_water_mark > 0) {
                global_status.logger_error->warn("Reached the high water mark, consuming message but not analysing it");
            } else {
                global_status.logger_console->info("Waveform message to be analyzed of size: {}", size);

                spdlog::stopwatch stopwatch;

                size_t waveforms_number = 0;

                std::vector<struct event_PSD> output_events;
                std::vector<uint8_t> output_waveforms;

                // We reserve the memory for the output events using a big enough
                // number, to reduce it we arbitrarily divide it by the event size.
                output_events.reserve(size / sizeof(struct event_PSD));
                // We reserve the memory for the output buffer twice as big as the
                // input buffer that is most likely to be big enough for the output
                // waveforms data as the gates are half as big as the samples.
                output_waveforms.reserve(size * defaults_waan_waveforms_buffer_multiplier);

                std::map<unsigned int, bool> disabled_already_warned;

                size_t input_offset = 0;

                // We add 14 bytes to the input_offset because we want to be sure to read at least
                // the first header of the waveform
                while ((input_offset + waveform_header_size()) < size)
                {
                    global_status.logger_console->debug("Message size: {}; input_offset: {}", size, input_offset);

                    uint64_t timestamp = *((uint64_t *)(buffer_input + input_offset));
                    const uint8_t this_channel = *((uint8_t *)(buffer_input + input_offset + 8));
                    const uint32_t samples_number = *((uint32_t *)(buffer_input + input_offset + 9));
                    const uint8_t gates_number = *((uint8_t *)(buffer_input + input_offset + 13));

                    global_status.logger_console->debug("Channel: {}; number of samples: {}", this_channel, samples_number);

                    const bool is_active = std::find(global_status.active_channels.begin(),
                                                     global_status.active_channels.end(),
                                                     this_channel) != global_status.active_channels.end();
                    const size_t needed_offset = input_offset + waveform_header_size()
                                               + (samples_number * sizeof(uint16_t))
                                               + (samples_number * gates_number * sizeof(uint8_t));

                    if (!is_active) {
                        if (!disabled_already_warned[this_channel]) {
                            global_status.logger_console->warn("Channel {} is disabled", this_channel);
                            disabled_already_warned[this_channel] = true;
                        }

                        global_status.disabled_channels.insert(this_channel);
                    }

                    if  (needed_offset > size) {
                        global_status.logger_error->warn("Incomplete waveform in buffer; needed_offset: {}; size: {}", needed_offset, size);
                    }

                    if (is_active && (needed_offset <= size)) {
                        global_status.logger_console->debug("Channel {} is active, reading samples...", this_channel);

                        waveforms_number += 1;

                        const uint16_t *samples = (uint16_t *)(buffer_input + input_offset + waveform_header_size());
                        // Should I store the additionals to the waveform?
                        // They are not standard and not quantitative, let's not bother...
                        // Actually, they might be useful as users might have
                        // stored important information in them.
                        const uint8_t *gates = (uint8_t *)(buffer_input + input_offset + waveform_header_size() + samples_number * sizeof(uint16_t));

                        struct event_waveform this_waveform = waveform_create(timestamp,
                                                                              this_channel,
                                                                              samples_number,
                                                                              gates_number);

                        waveform_samples_set(&this_waveform, samples);

                        for (uint8_t i = 0; i < gates_number; i++) {
                            waveform_additional_set(&this_waveform,
                                                    i,
                                                    gates + samples_number * i * sizeof(uint8_t));
                        }

                        global_status.logger_console->debug("Allocating events buffer");

                        struct event_PSD *events_buffer = (struct event_PSD *)calloc(1, sizeof(struct event_PSD));
                        uint32_t *trigger_positions = (uint32_t*)calloc(1, sizeof(uint32_t));
                        size_t events_number = 1;

                        events_buffer[0].timestamp = timestamp;
                        events_buffer[0].channel = this_channel;

                        global_status.logger_console->debug("Timestamp analysis");

                        global_status.channels_timestamp_analysis[this_channel].
                            fn(samples,
                               samples_number,
                               &this_waveform,
                               &trigger_positions,
                               &events_buffer,
                               &events_number,
                               global_status.channels_timestamp_user_config[this_channel]);

                        global_status.logger_console->debug("Channel {}; samples_number: {}, events_number: {}", this_channel, samples_number, events_number);

                        global_status.logger_console->debug("Energy analysis");

                        global_status.channels_energy_analysis[this_channel].
                            fn(samples,
                               samples_number,
                               &this_waveform,
                               &trigger_positions,
                               &events_buffer,
                               &events_number,
                               global_status.channels_energy_user_config[this_channel]);

                        global_status.logger_console->debug("Channel {}; samples_number: {}, events_number: {}", this_channel, samples_number, events_number);

                        if (events_number > 0 && global_status.forward_waveforms) {
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

                        if (events_number > 0) {
                            global_status.partial_counts[this_channel] += events_number;

                            const size_t current_events_buffer_size = output_events.size();

                            output_events.resize(current_events_buffer_size + events_number);

                            memcpy(output_events.data() + current_events_buffer_size,
                                   events_buffer,
                                   events_number * sizeof(struct event_PSD));
                        }

                        global_status.logger_console->debug("Channel {}; samples_number: {}, events_number: {}", this_channel, samples_number, events_number);

                        if (trigger_positions) {
                            free(trigger_positions);
                            trigger_positions = NULL;
                        }
                        if (events_buffer) {
                            free(events_buffer);
                            events_buffer = NULL;
                        }

                        waveform_destroy_samples(&this_waveform);
                    }

                    // Compute the waveform event size
                    const size_t this_size = waveform_header_size() + samples_number * sizeof(uint16_t) + gates_number * samples_number * sizeof(uint8_t);
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

                    global_status.logger_console->info("Sending waveforms buffer; Topic: {}; buffer size: {}", topic, total_waveforms_size);

                    const int result = send_byte_message(global_status.data_output_socket,
                                                         topic.c_str(),
                                                         reinterpret_cast<void*>(output_waveforms.data()),
                                                         total_waveforms_size, 0);

                    global_status.waveforms_msg_ID += 1;

                    if (result == EXIT_FAILURE)
                    {
                        global_status.logger_error->error("ZeroMQ Error on publishing events' waveforms");
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

                    global_status.logger_console->info("Sending events buffer; Topic: {}; buffer size: {}", topic, total_events_size);


                    const int result = send_byte_message(global_status.data_output_socket,
                                                         topic.c_str(),
                                                         reinterpret_cast<void*>(output_events.data()),
                                                         total_events_size, 0);

                    global_status.events_msg_ID += 1;

                    if (result == EXIT_FAILURE)
                    {
                        global_status.logger_error->error("ZeroMQ Error on publishing events' event_PSDs");
                    }
                }

                const auto elapsed = stopwatch.elapsed();
                const double elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();

                global_status.logger_console->info("Elaboration end; elapsed: {:.2f} ms; number of waveforms: {}; elaboration speed: {:.2f} events/s", elapsed_ns / 1e6, waveforms_number, waveforms_number / (elapsed_ns / 1e6));
            }
        }

        free(topic);
        free(buffer_input);

        topic = NULL;
        buffer_input = NULL;

        if (global_status.data_input_source == RAW_FILE_INPUT) {
            result = read_byte_message_from_adr(data_input_file, &topic, (void **)(&buffer_input), &size, true, 0);
        } else {
            result = receive_byte_message(data_input_socket, &topic, (void **)(&buffer_input), &size, true, 0);
        }
    }

    if (topic) {
        free(topic);
    }
    if (buffer_input) {
        free(buffer_input);
    }

    if (result == EXIT_FAILURE && global_status.data_input_source == SOCKET_INPUT) {
        return states::COMMUNICATION_ERROR;
    } else if (result == EXIT_FAILURE) {
        return states::FILE_READING_ERROR;
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
    FILE *data_input_file = global_status.data_input_file;

    const int s = zmq_close(status_socket);
    if (s != 0)
    {
        global_status.logger_error->error("ZeroMQ Error on status socket close");
    }

    const int i = zmq_close(data_input_socket);
    if (i != 0)
    {
        global_status.logger_error->error("ZeroMQ Error on data input socket close");
    }

    const int o = zmq_close(data_output_socket);
    if (o != 0)
    {
        global_status.logger_error->error("ZeroMQ Error on data output socket close");
    }

    const int c = zmq_close(commands_socket);
    if (c != 0)
    {
        global_status.logger_error->error("ZeroMQ Error on commands socket close");
    }

    if (data_input_file) {
        fclose(data_input_file);
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
        global_status.logger_error->error("ZeroMQ Error on context destroy");
    }

    return states::STOP;
}

state actions::stop(status&)
{
    return states::STOP;
}

state actions::file_reading_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));

    if (feof(global_status.data_input_file)) {
        json_object_set_new_nocheck(json_event_message, "error", json_string("Reached EOF (End Of File)"));
    } else {
        json_object_set_new_nocheck(json_event_message, "error", json_string("File reading error"));
    }

    actions::generic::publish_message(global_status, defaults_waan_events_topic, json_event_message);

    json_decref(json_event_message);

    // When waan reaches the EOF of the data_input_file there might still be
    // messages in the outgoing queue, so we put a long(ish) delay to allow the
    // messages to be delivered
    std::this_thread::sleep_for(std::chrono::milliseconds(defaults_waan_zmq_flush_delay));

    return states::CLOSE_SOCKETS;
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
