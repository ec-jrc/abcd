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

#include <cstring>
#include <chrono>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <string>
#include <zmq.h>
#include <jansson.h>

extern "C" {
#include "utilities_functions.h"
#include "socket_functions.h"
#include "jansson_socket_functions.h"
}

#include "typedefs.hpp"
#include "states.hpp"
#include "events.hpp"
#include "actions.hpp"

#define BUFFER_SIZE 32

/******************************************************************************/
/* Generic actions                                                            */
/******************************************************************************/

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

    json_object_set_new(status_message, "module", json_string("dasa"));
    json_object_set_new(status_message, "timestamp", json_string(time_buffer));
    json_object_set_new(status_message, "msg_ID", json_integer(status_msg_ID));

    send_json_message(status_socket, const_cast<char*>(topic.c_str()), status_message, 1);

    global_status.status_msg_ID += 1;
}

void actions::generic::close_file(status &global_status)
{
    if (global_status.events_output_file.is_open())
    {
        global_status.events_output_file.close();
        global_status.events_output_file.clear();
    }

    if (global_status.waveforms_output_file.is_open())
    {
        global_status.waveforms_output_file.close();
        global_status.waveforms_output_file.clear();
    }

    if (global_status.raw_output_file.is_open())
    {
        global_status.raw_output_file.close();
        global_status.raw_output_file.clear();
    }
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

    // Creates the SUB data socket
    void *abcd_data_socket = zmq_socket(context, ZMQ_SUB);
    if (!abcd_data_socket)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on abcd data socket creation: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Creates the SUB status socket
    void *abcd_status_socket = zmq_socket(context, ZMQ_SUB);
    if (!abcd_status_socket)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on abcd status socket creation: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Creates the SUB status socket
    void *waan_status_socket = zmq_socket(context, ZMQ_SUB);
    if (!waan_status_socket)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on waan status socket creation: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    global_status.status_socket = status_socket;
    global_status.abcd_data_socket = abcd_data_socket;
    global_status.abcd_status_socket = abcd_status_socket;
    global_status.waan_status_socket = waan_status_socket;
    global_status.commands_socket = commands_socket;

    return states::BIND_SOCKETS;
}

state actions::bind_sockets(status &global_status)
{
    std::string status_address = global_status.status_address;
    std::string commands_address = global_status.commands_address;
    std::string abcd_data_address = global_status.abcd_data_address;
    std::string abcd_status_address = global_status.abcd_status_address;
    std::string waan_status_address = global_status.waan_status_address;

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

    // Connects the abcd data socket to its address
    const int ad = zmq_connect(global_status.abcd_data_socket, abcd_data_address.c_str());
    if (ad != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on abcd data socket connection: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Connects the abcd status socket to its address
    const int as = zmq_connect(global_status.abcd_status_socket, abcd_status_address.c_str());
    if (as != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on abcd status socket connection: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Connects the waan status socket to its address
    const int ws = zmq_connect(global_status.waan_status_socket, waan_status_address.c_str());
    if (ws != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on waan status socket connection: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Subscribe to all topics for the data stream
    std::string abcd_data_topic("");
    zmq_setsockopt(global_status.abcd_data_socket, ZMQ_SUBSCRIBE, abcd_data_topic.c_str(), abcd_data_topic.size());

    // Subscribe to abcd status and events topics
    std::string abcd_status_topic("");
    zmq_setsockopt(global_status.abcd_status_socket, ZMQ_SUBSCRIBE, abcd_status_topic.c_str(), abcd_status_topic.size());

    // Subscribe to waan status and events topics
    std::string waan_status_topic("");
    zmq_setsockopt(global_status.waan_status_socket, ZMQ_SUBSCRIBE, waan_status_topic.c_str(), waan_status_topic.size());

    return states::PUBLISH_STATUS;
}

state actions::publish_status(status &global_status)
{
    json_t* status_message = json_object();

    json_object_set_new(status_message, "events_file_opened", json_boolean(false));
    json_object_set_new(status_message, "waveforms_file_opened", json_boolean(false));
    json_object_set_new(status_message, "raw_file_opened", json_boolean(false));

    actions::generic::publish_message(global_status, defaults_lmno_status_topic, status_message);

    json_decref(status_message);

    return states::EMPTY_QUEUE;
}

state actions::empty_queue(status &global_status)
{
    void *abcd_data_socket = global_status.abcd_data_socket;

    char *buffer;
    size_t size;

    receive_byte_message(abcd_data_socket, nullptr, (void **)(&buffer), &size, 0, 0);

    while (size > 0)
    {
        if (global_status.verbosity > 1)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "abcd data message; ";
            std::cout << "Size: " << size << "; ";
            std::cout << std::endl;
        }

        free(buffer);

        receive_byte_message(abcd_data_socket, nullptr, (void **)(&buffer), &size, 0, 0);
    }

    void *abcd_status_socket = global_status.abcd_status_socket;

    receive_byte_message(abcd_status_socket, nullptr, (void **)(&buffer), &size, 0, 0);

    while (size > 0)
    {
        if (global_status.verbosity > 1)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "abcd status message; ";
            std::cout << "Size: " << size << "; ";
            std::cout << std::endl;
        }

        free(buffer);

        receive_byte_message(abcd_status_socket, nullptr, (void **)(&buffer), &size, 0, 0);
    }

    void *waan_status_socket = global_status.waan_status_socket;

    receive_byte_message(waan_status_socket, nullptr, (void **)(&buffer), &size, 0, 0);

    while (size > 0)
    {
        if (global_status.verbosity > 1)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "waan status message; ";
            std::cout << "Size: " << size << "; ";
            std::cout << std::endl;
        }

        free(buffer);

        receive_byte_message(waan_status_socket, nullptr, (void **)(&buffer), &size, 0, 0);
    }

    return states::RECEIVE_COMMANDS;
}

state actions::receive_commands(status &global_status)
{
    void *commands_socket = global_status.commands_socket;

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

            if (command == std::string("start") && json_arguments != NULL)
            {
                json_t *json_file_name = json_object_get(json_arguments, "file_name");
                json_t *json_enable = json_object_get(json_arguments, "enable");

                if (json_file_name != NULL && json_enable != NULL)
                {
                    const std::string file_name = json_string_value(json_file_name);

                    if (file_name.length() > 0)
                    {
                        const std::size_t found = file_name.find_last_of(".");

                        std::string root_file_name;

                        if (found != 0 && found != std::string::npos)
                        {
                            root_file_name = file_name.substr(0, found);
                        }
                        else
                        {
                            root_file_name = file_name;
                        }

                        bool events_enabled = json_is_true(json_object_get(json_enable, "events"));

                        bool waveforms_enabled = json_is_true(json_object_get(json_enable, "waveforms"));
                        bool raw_enabled = json_is_true(json_object_get(json_enable, "raw"));

                        global_status.events_file_name.clear();
                        if (events_enabled)
                        {
                            global_status.events_file_name = root_file_name;
                            global_status.events_file_name.append("_events.");
                            global_status.events_file_name.append(defaults_lmno_extenstion_events);
                        }

                        global_status.waveforms_file_name.clear();
                        if (waveforms_enabled)
                        {
                            global_status.waveforms_file_name = root_file_name;
                            global_status.waveforms_file_name.append("_waveforms.");
                            global_status.waveforms_file_name.append(defaults_lmno_extenstion_waveforms);
                        }

                        global_status.raw_file_name.clear();
                        if (raw_enabled)
                        {
                            global_status.raw_file_name = root_file_name;
                            global_status.raw_file_name.append("_raw.");
                            global_status.raw_file_name.append(defaults_lmno_extenstion_raw);
                        }

                        if (global_status.verbosity > 0)
                        {
                            char time_buffer[BUFFER_SIZE];
                            time_string(time_buffer, BUFFER_SIZE, NULL);
                            std::cout << '[' << time_buffer << "] ";
                            std::cout << "Received file name: " << file_name << "; ";
                            std::cout << "root: " << root_file_name << "; ";
                            std::cout << "events_enabled: " << events_enabled << "; ";
                            std::cout << "events_file_name: " << global_status.events_file_name << "; ";
                            std::cout << "waveforms_enabled: " << waveforms_enabled << "; ";
                            std::cout << "waveforms_file_name: " << global_status.waveforms_file_name << "; ";
                            std::cout << "raw_enabled: " << raw_enabled << "; ";
                            std::cout << "raw_file_name: " << global_status.raw_file_name << "; ";
                            std::cout << std::endl;
                        }

                        if (events_enabled || waveforms_enabled || raw_enabled)
                        {
                            return states::OPEN_FILE;
                        }
                    }
                }
            }
            else if (command == std::string("quit"))
            {
                return states::STOP_CLOSE_FILE;
            }
        }
    }

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    if (now - global_status.last_publication > std::chrono::seconds(defaults_lmno_publish_timeout))
    {
        return states::PUBLISH_STATUS;
    }

    return states::EMPTY_QUEUE;
}

state actions::open_file(status &global_status)
{
    // Clear all the counters
    global_status.events_file_size = 0;
    global_status.waveforms_file_size = 0;
    global_status.raw_file_size = 0;

    // Open the available files
    const std::string events_file_name = global_status.events_file_name;
    const std::string waveforms_file_name = global_status.waveforms_file_name;
    const std::string raw_file_name = global_status.raw_file_name;

    std::string message("Opening files: ");

    if (events_file_name.size() > 0)
    {
        message.append(events_file_name);
        message.append(", ");
    }

    if (waveforms_file_name.size() > 0)
    {
        message.append(waveforms_file_name);
        message.append(", ");
    }

    if (raw_file_name.size() > 0)
    {
        message.append(raw_file_name);
        message.append(", ");
    }

    json_t* status_message = json_object();

    json_object_set_new(status_message, "type", json_string("event"));
    json_object_set_new(status_message, "event", json_string(message.c_str()));

    actions::generic::publish_message(global_status, defaults_lmno_events_topic, status_message);

    json_decref(status_message);

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Opening file: " << events_file_name << "; ";
        std::cout << "Opening file: " << waveforms_file_name << "; ";
        std::cout << "Opening file: " << raw_file_name << "; ";
        std::cout << std::endl;
    }

    try
    {
        if (global_status.events_output_file.is_open())
        {
            global_status.events_output_file.close();
            global_status.events_output_file.clear();
        }

        global_status.events_output_file.open(events_file_name, std::ios::out | std::ios::binary);

        if (!global_status.events_output_file.is_open())
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR, unable to open: " << events_file_name << std::endl;
        }

        if (!global_status.events_output_file.good())
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR, events bad file: " << events_file_name << std::endl;

            global_status.events_output_file.close();
        }
    }
    catch (const std::exception &e)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR, unable to create events file: " << e.what() << std::endl;
    }

    try
    {
        if (global_status.waveforms_output_file.is_open())
        {
            global_status.waveforms_output_file.close();
            global_status.waveforms_output_file.clear();
        }

        global_status.waveforms_output_file.open(waveforms_file_name, std::ios::out | std::ios::binary);

        if (!global_status.waveforms_output_file.is_open())
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR, unable to open: " << waveforms_file_name << std::endl;
        }

        if (!global_status.waveforms_output_file.good())
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR, waveforms bad file: " << waveforms_file_name << std::endl;

            global_status.waveforms_output_file.close();
        }
    }
    catch (const std::exception &e)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR, unable to create waveforms file: " << e.what() << std::endl;
    }

    try
    {
        if (global_status.raw_output_file.is_open())
        {
            global_status.raw_output_file.close();
            global_status.raw_output_file.clear();
        }

        global_status.raw_output_file.open(raw_file_name, std::ios::out | std::ios::binary);

        if (!global_status.raw_output_file.is_open())
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR, unable to open: " << raw_file_name << std::endl;
        }

        if (!global_status.raw_output_file.good())
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR, raw bad file: " << raw_file_name << std::endl;

            global_status.raw_output_file.close();
        }
    }
    catch (const std::exception &e)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR, unable to create raw file: " << e.what() << std::endl;
    }

    global_status.start_time = std::chrono::system_clock::now();

    if (global_status.events_output_file.good()
        || global_status.waveforms_output_file.good()
        || global_status.raw_output_file.good())
    {
        return states::WRITE_DATA;
    }
    else
    {
        return states::IO_ERROR;
    }
}

state actions::write_data(status &global_status)
{
    void *abcd_data_socket = global_status.abcd_data_socket;
    void *abcd_status_socket = global_status.abcd_status_socket;
    void *waan_status_socket = global_status.waan_status_socket;

    char *buffer;
    size_t size;

    receive_byte_message(abcd_status_socket, nullptr, (void **)(&buffer), &size, 0, 0);

    while (size > 0)
    {
        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "abcd status message; ";
            std::cout << "Size: " << size << "; ";
            std::cout << std::endl;
        }

        if (global_status.raw_output_file.good())
        {
            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Saving abcd status message to raw file: " << global_status.raw_file_name << "; ";
                std::cout << "Data size: " << size << "; ";
                std::cout << std::endl;
            }

            global_status.raw_output_file.write(reinterpret_cast<const char*>(buffer), size);
            global_status.raw_file_size += size;
        }

        free(buffer);

        receive_byte_message(abcd_status_socket, nullptr, (void **)(&buffer), &size, 0, 0);
    }

    receive_byte_message(waan_status_socket, nullptr, (void **)(&buffer), &size, 0, 0);

    while (size > 0)
    {
        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "waan status message; ";
            std::cout << "Size: " << size << "; ";
            std::cout << std::endl;
        }

        if (global_status.raw_output_file.good())
        {
            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Saving waan status message to raw file: " << global_status.raw_file_name << "; ";
                std::cout << "Data size: " << size << "; ";
                std::cout << std::endl;
            }

            global_status.raw_output_file.write(reinterpret_cast<const char*>(buffer), size);
            global_status.raw_file_size += size;
        }

        free(buffer);

        receive_byte_message(waan_status_socket, nullptr, (void **)(&buffer), &size, 0, 0);
    }

    receive_byte_message(abcd_data_socket, nullptr, (void **)(&buffer), &size, 0, 0);

    while (size > 0)
    {
        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Message size: " << size << "; ";
            std::cout << std::endl;
        }

        char *char_buffer = reinterpret_cast<char*>(buffer);

        const char *position = strchr(char_buffer, ' ');
        const size_t topic_length = position - char_buffer;

        std::string topic(char_buffer, topic_length);

        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Topic: " << topic << "; ";
            std::cout << "Topic length: " << topic_length << "; ";
            std::cout << std::endl;
        }

        if (global_status.raw_output_file.good())
        {
            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Saving events message to raw file: " << global_status.raw_file_name << "; ";
                std::cout << "Data size: " << size << "; ";
                std::cout << std::endl;
            }

            global_status.raw_output_file.write(char_buffer, size);
            global_status.raw_file_size += size;
        }

        if (global_status.events_output_file.good()
            &&
            (topic.compare(0, strlen(defaults_abcd_data_events_topic), defaults_abcd_data_events_topic) == 0))
        {
            const size_t data_size = size - topic.size() - 1;
            const size_t events_number = data_size / sizeof(event_PSD);

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Saving to events file: " << global_status.events_file_name << "; ";
                std::cout << "Data size: " << data_size << "; ";
                std::cout << "Events number: " << events_number << "; ";
                std::cout << "mod: " << data_size % sizeof(event_PSD) << "; ";
                std::cout << std::endl;
            }

            const char *pointer = reinterpret_cast<const char*>(position + 1);
            global_status.events_output_file.write(pointer, data_size);
            global_status.events_file_size += data_size;
        }
        else if (global_status.waveforms_output_file.good()
                 &&
                 (topic.compare(0, strlen(defaults_abcd_data_waveforms_topic), defaults_abcd_data_waveforms_topic) == 0))
        {
            const size_t data_size = size - topic.size() - 1;

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Data size: " << data_size << "; ";
                std::cout << std::endl;
            }

            const char *pointer = reinterpret_cast<const char*>(position + 1);
            global_status.waveforms_output_file.write(pointer, data_size);
            global_status.waveforms_file_size += data_size;
        }

        free(buffer);

        receive_byte_message(abcd_data_socket, nullptr, (void **)(&buffer), &size, 0, 0);
    }

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    if (now - global_status.last_publication > std::chrono::seconds(defaults_lmno_publish_timeout))
    {
        return states::FLUSH_FILE;
    }

    return states::WRITE_DATA;
}

state actions::flush_file(status &global_status)
{
    if (global_status.events_output_file.good())
    {
        global_status.events_output_file.flush();
    }
    else if (global_status.events_file_name.size() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR, events file is not good" << std::endl;
    }

    if (global_status.waveforms_output_file.good())
    {
        global_status.waveforms_output_file.flush();
    }
    else if (global_status.waveforms_file_name.size() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR, events file is not good" << std::endl;
    }

    if (global_status.raw_output_file.good())
    {
        global_status.raw_output_file.flush();
    }
    else if (global_status.raw_file_name.size() > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR, raw file is not good" << std::endl;
    }

    return states::SAVING_PUBLISH_STATUS;
}


state actions::saving_publish_status(status &global_status)
{
    json_t* status_message = json_object();

    const auto now = std::chrono::system_clock::now();

    const auto run_delta_time = std::chrono::duration_cast<std::chrono::duration<long int>>(now - global_status.start_time);
    const long int runtime = run_delta_time.count();

    json_object_set_new(status_message, "runtime", json_integer(runtime));

    json_object_set_new(status_message, "events_file_opened", json_boolean(global_status.events_output_file.good()));
    json_object_set_new(status_message, "waveforms_file_opened", json_boolean(global_status.waveforms_output_file.good()));
    json_object_set_new(status_message, "raw_file_opened", json_boolean(global_status.raw_output_file.good()));

    json_object_set_new(status_message, "events_file_name", json_string(global_status.events_file_name.c_str()));
    json_object_set_new(status_message, "waveforms_file_name", json_string(global_status.waveforms_file_name.c_str()));
    json_object_set_new(status_message, "raw_file_name", json_string(global_status.raw_file_name.c_str()));

    json_object_set_new(status_message, "events_file_size", json_integer(global_status.events_file_size));
    json_object_set_new(status_message, "waveforms_file_size", json_integer(global_status.waveforms_file_size));
    json_object_set_new(status_message, "raw_file_size", json_integer(global_status.raw_file_size));

    actions::generic::publish_message(global_status, defaults_lmno_status_topic, status_message);

    json_decref(status_message);

    return states::SAVING_RECEIVE_COMMANDS;
}

state actions::saving_receive_commands(status &global_status)
{
    void *commands_socket = global_status.commands_socket;

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

            if (command == std::string("stop"))
            {
                return states::CLOSE_FILE;
            }
        }
    }

    return states::WRITE_DATA;
}

state actions::close_file(status &global_status)
{
    json_t* status_message = json_object();

    const auto now = std::chrono::system_clock::now();

    const auto run_delta_time = std::chrono::duration_cast<std::chrono::duration<long int>>(now - global_status.start_time);
    const long int runtime = run_delta_time.count();

    char events_file_size[BUFFER_SIZE];
    file_size_to_human(events_file_size, BUFFER_SIZE, global_status.events_file_size);

    char waveforms_file_size[BUFFER_SIZE];
    file_size_to_human(waveforms_file_size, BUFFER_SIZE, global_status.waveforms_file_size);

    char raw_file_size[BUFFER_SIZE];
    file_size_to_human(raw_file_size, BUFFER_SIZE, global_status.raw_file_size);


    std::string message = "Closing files (duration: ";
    message.append(std::to_string(runtime));
    message.append("s, events: ");
    message.append(events_file_size);
    message.append(", waveforms: ");
    message.append(waveforms_file_size);
    message.append(", raw: ");
    message.append(raw_file_size);
    message.append(")");

    json_object_set_new(status_message, "runtime", json_integer(runtime));

    json_object_set_new(status_message, "events_file_name", json_string(global_status.events_file_name.c_str()));
    json_object_set_new(status_message, "waveforms_file_name", json_string(global_status.waveforms_file_name.c_str()));
    json_object_set_new(status_message, "raw_file_name", json_string(global_status.raw_file_name.c_str()));

    json_object_set_new(status_message, "events_file_size", json_integer(global_status.events_file_size));
    json_object_set_new(status_message, "waveforms_file_size", json_integer(global_status.waveforms_file_size));
    json_object_set_new(status_message, "raw_file_size", json_integer(global_status.raw_file_size));

    json_object_set_new(status_message, "type", json_string("event"));
    json_object_set_new(status_message, "event", json_string(message.c_str()));

    actions::generic::publish_message(global_status, defaults_lmno_events_topic, status_message);

    json_decref(status_message);

    actions::generic::close_file(global_status);

    return states::RECEIVE_COMMANDS;
}

state actions::stop_close_file(status &global_status)
{
    actions::generic::close_file(global_status);

    return states::CLOSE_SOCKETS;
}

state actions::close_sockets(status &global_status)
{
    const int s = zmq_close(global_status.status_socket);
    if (s != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on status socket close: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    const int c = zmq_close(global_status.commands_socket);
    if (c != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on commands socket close: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    const int ad = zmq_close(global_status.abcd_data_socket);
    if (ad != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on abcd data socket close: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    const int as = zmq_close(global_status.abcd_status_socket);
    if (as != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on abcd status socket close: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    const int ws = zmq_close(global_status.waan_status_socket);
    if (ws != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on waan status socket close: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    return states::DESTROY_CONTEXT;
}

state actions::destroy_context(status &global_status)
{
    const int c = zmq_ctx_destroy(global_status.context);
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
    json_t* status_message = json_object();

    json_object_set_new(status_message, "type", json_string("event"));
    json_object_set_new(status_message, "error", json_string("Communication error"));

    actions::generic::publish_message(global_status, defaults_lmno_events_topic, status_message);

    json_decref(status_message);

    return states::CLOSE_SOCKETS;
}

state actions::io_error(status &global_status)
{
    json_t* status_message = json_object();

    json_object_set_new(status_message, "type", json_string("event"));
    json_object_set_new(status_message, "error", json_string("I/O error"));

    actions::generic::publish_message(global_status, defaults_lmno_events_topic, status_message);

    json_decref(status_message);

    return states::CLOSE_FILE;
}

state actions::stop(status&)
{
    return states::STOP;
}