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

#include "class_caen_hv.h"
#include "socket_functions.h"

#include "utilities_functions.hpp"
#include "typedefs.hpp"
#include "states.hpp"
#include "actions.hpp"

/******************************************************************************/
/* Generic actions                                                            */
/******************************************************************************/

void actions::generic::publish_status(status &global_status,
                                      std::string topic,
                                      json_t *status_message)
{
    void *status_socket = global_status.status_socket;
    const unsigned long int status_msg_ID = global_status.status_msg_ID;

    json_object_set_new(status_message, "module", json_string("hijk"));
    json_object_set_new(status_message, "timestamp", json_string(utilities_functions::time_string().c_str()));
    json_object_set_new(status_message, "msg_ID", json_integer(status_msg_ID));

    char *output_buffer = json_dumps(status_message, JSON_COMPACT);

    if (output_buffer != NULL) {
        if (global_status.verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "Status buffer: ";
            std::cout << output_buffer;
            std::cout << std::endl;
        }

        send_byte_message(status_socket, topic.c_str(), output_buffer, strlen(output_buffer), 1);

        free(output_buffer);
    }

    global_status.status_msg_ID += 1;
}

bool actions::generic::create_hv(status &global_status)
{
    const json_t *json_model = json_object_get(global_status.config, "model");

    int model = defaults_hijk_model;

    if (json_model != NULL && json_is_integer(json_model))
    {
        // FIXME This has to be using strings
        model = json_integer_value(json_model);
    }

    if (global_status.verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Creating model: ";
        std::cout << model;
        std::cout << std::endl;
    }

    try
    {
        CAENHV *hv_card = new CAENHV(model);
        global_status.hv_card = hv_card;
    }
    catch (const std::bad_alloc &)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Unable to instantiate HV card";
        std::cout << std::endl;

        return false;
    }

    return true;
}

void actions::generic::destroy_hv(status &global_status)
{
    if (global_status.hv_card)
    {
        global_status.hv_card->Deactivate();

        delete global_status.hv_card;
    }

    global_status.hv_card = nullptr;
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
        std::cout << '[' << utilities_functions::time_string() << "] ";
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
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: ZeroMQ Error on status socket creation: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Creates the commands socket
    void *commands_socket = zmq_socket(context, ZMQ_PULL);
    if (!commands_socket)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: ZeroMQ Error on commands socket creation: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    global_status.status_socket = status_socket;
    global_status.commands_socket = commands_socket;

    return states::BIND_SOCKETS;
}

state actions::bind_sockets(status &global_status)
{
    std::string status_address = global_status.status_address;
    std::string commands_address = global_status.commands_address;

    // Binds the status socket to its address
    const int s = zmq_bind(global_status.status_socket, status_address.c_str());
    if (s != 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: ZeroMQ Error on status socket binding: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Binds the socket to its address
    const int c = zmq_bind(global_status.commands_socket, commands_address.c_str());
    if (c != 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: ZeroMQ Error on commands socket binding: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    return states::READ_CONFIG;
}

state actions::read_config(status &global_status)
{
    std::string config_file = global_status.config_file;

    if (global_status.verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Reading file: ";
        std::cout << config_file.c_str();
        std::cout << std::endl;
    }

    json_error_t error;

    json_t *new_config = json_load_file(config_file.c_str(), 0, &error);

    if (!new_config)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
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

        return states::IO_ERROR;
    }

    global_status.config = new_config;

    return states::CREATE_HV;
}

state actions::create_hv(status &global_status)
{
    json_t *status_message = json_object();

    json_object_set_new(status_message, "type", json_string("event"));
    json_object_set_new(status_message, "event", json_string("HV initialization"));

    actions::generic::publish_status(global_status, defaults_hijk_events_topic, status_message);

    json_decref(status_message);

    const bool success = actions::generic::create_hv(global_status);

    if (success)
    {
        return states::INITIALIZE_HV;
    }
    else
    {
        return states::CONFIGURE_ERROR;
    }
}

state actions::recreate_hv(status &global_status)
{
    const bool success = actions::generic::create_hv(global_status);

    if (success)
    {
        return states::INITIALIZE_HV;
    }
    else
    {
        return states::CONFIGURE_ERROR;
    }
}

state actions::reconfigure_destroy_hv(status &global_status)
{
    actions::generic::destroy_hv(global_status);

    return states::RECREATE_HV;
}


state actions::initialize_hv(status &global_status)
{
    CAENHV *hv_card = global_status.hv_card;

    if (!hv_card)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: HV pointer unavailable";
        std::cout << std::endl;

        return states::CONFIGURE_ERROR;
    }

    json_t *config = global_status.config;

    int model = global_status.model;

    json_t *json_model = json_object_get(config, "model");

    if (json_model != NULL && json_is_integer(json_model))
    {
        // FIXME This has to be using strings
        model = json_integer_value(json_model);
    }
    else
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Wrong format of HV model; ";
        std::cout << std::endl;

        return states::CONFIGURE_ERROR;
    }

    const std::string string_connection_type = json_string_value(json_object_get(config, "connection_type"));

    // This is an enum class
    CAEN_Comm_ConnectionType connection_type;
    if (string_connection_type == "PCI")
    {
        connection_type = CAENComm_OpticalLink;
    }
    else if (string_connection_type == "PCIe")
    {
        connection_type = CAENComm_OpticalLink;
    }
    else if (string_connection_type == "OpticalLink")
    {
        connection_type = CAENComm_OpticalLink;
    }
    else if (string_connection_type == "USB")
    {
        connection_type = CAENComm_USB;
    }
    else
    {
        connection_type = static_cast<CAEN_Comm_ConnectionType>(global_status.connection_type);
    }

    unsigned int link_number = global_status.link_number;

    json_t *json_link_number = json_object_get(config, "link_number");

    if (json_link_number != NULL && json_is_integer(json_link_number))
    {
        link_number = json_integer_value(json_link_number);
    }
    else
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Wrong format of link number; ";
        std::cout << std::endl;

        return states::CONFIGURE_ERROR;
    }

    unsigned int CONET_node = global_status.CONET_node;

    json_t *json_CONET_node = json_object_get(config, "CONET_node");

    if (json_CONET_node != NULL && json_is_integer(json_CONET_node))
    {
        CONET_node = json_integer_value(json_object_get(config, "CONET_node"));
    }
    else
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Wrong format of CONET node; ";
        std::cout << std::endl;

        return states::CONFIGURE_ERROR;
    }

    uint32_t VME_address = global_status.VME_address;

    json_t *json_VME_address = json_object_get(config, "VME_address");

    if (json_VME_address != NULL && json_is_string(json_VME_address))
    {
        const char *VME_address_string = json_string_value(json_object_get(config, "VME_address"));

        if (global_status.verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "VME address string: ";
            std::cout << VME_address_string;
            std::cout << std::endl;
        }

        try
        {
            // Putting all the arguments to the function to read hex numbers
            VME_address = std::stoi(VME_address_string, nullptr, 0);
        }
        catch (...)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "ERROR: Wrong format of VME address";
            std::cout << std::endl;

            return states::CONFIGURE_ERROR;
        }
    }
    else if (json_VME_address != NULL && json_is_integer(json_VME_address))
    {
        VME_address = json_integer_value(json_object_get(config, "VME_address"));
    }
    else
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Wrong format of VME address";
        std::cout << std::endl;

        return states::CONFIGURE_ERROR;
    }

    if (global_status.verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "VME address: ";
        std::cout << VME_address;
        std::cout << std::endl;
    }


    unsigned int verbosity = global_status.verbosity;

    if (hv_card->IsActive() == 1)
    {
        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "Card already active, deactivating it";
            std::cout << std::endl;
        }
        hv_card->Deactivate();
    }

    if (verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Initializing card model; ";
        std::cout << std::endl;
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Model number: " << model << "; ";
        std::cout << std::endl;
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Connection type: " << connection_type << "; ";
        std::cout << std::endl;
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Link number: " << link_number << "; ";
        std::cout << std::endl;
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "CONET node: " << CONET_node << "; ";
        std::cout << std::endl;
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "VME address: 0x" << std::hex << VME_address << std::dec << "; ";
        std::cout << std::endl;
    }
    hv_card->InitializeModel(model);

    if (hv_card->IsActive() == 0)
    {
        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "Activating card; ";
            std::cout << std::endl;
        }
        hv_card->Activate(connection_type, link_number, CONET_node, VME_address);
    }

    if (hv_card->IsActive() == 0)
    {
        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "ERROR: Unable to activate card";
            std::cout << std::endl;
        }
        hv_card->Deactivate();

        return states::CONFIGURE_ERROR;
    }
    else
    {
        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "Card activated";
            std::cout << std::endl;
        }
    }

    if (verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Description: " << hv_card->GetDescr() << "; ";
        std::cout << std::endl;
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Model: " << hv_card->GetModel() << "; ";
        std::cout << std::endl;
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Number of channels: " << hv_card->GetChNum() << "; ";
        std::cout << std::endl;
    }

    return states::CONFIGURE_HV;
}

state actions::configure_hv(status &global_status)
{
    CAENHV *hv_card = global_status.hv_card;

    if (!hv_card)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: HV pointer unavailable";
        std::cout << std::endl;

        return states::CONFIGURE_ERROR;
    }

    json_t *channels = json_object_get(global_status.config, "channels");
    const size_t channels_size = json_array_size(channels);

    for (size_t i = 0; i < channels_size; i++)
    {
        json_t *channel_config = json_array_get(channels, i);

        const int channel = json_integer_value(json_object_get(channel_config, "id"));

        const int max_current = json_integer_value(json_object_get(channel_config, "max_current"));
        const int max_voltage = json_integer_value(json_object_get(channel_config, "max_voltage"));
        const int ramp_up = json_integer_value(json_object_get(channel_config, "ramp_up"));
        const int ramp_down = json_integer_value(json_object_get(channel_config, "ramp_down"));

        hv_card->SetChannelCurrent(channel, max_current);
        hv_card->SetChannelSVMax(channel, max_voltage);
        hv_card->SetChannelRampUp(channel, ramp_up);
        hv_card->SetChannelRampDown(channel, ramp_down);

        if (global_status.verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "Settings for channel: " << channel << "; ";
            std::cout << std::endl;
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "Max current: " << max_current << "; ";
            std::cout << std::endl;
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "Max voltage: " << max_voltage << "; ";
            std::cout << std::endl;
        }

        if (json_is_null(json_object_get(channel_config, "on")))
        {
            if (global_status.verbosity > 0)
            {
                std::cout << '[' << utilities_functions::time_string() << "] ";
                std::cout << "Not modifying channel: " << channel << "; ";
                std::cout << std::endl;

                const bool previous_on = hv_card->IsChannelOn(channel);

                std::cout << '[' << utilities_functions::time_string() << "] ";
                std::cout << "Setting on status to: " << previous_on << "; ";
                std::cout << std::endl;

                json_object_set(channel_config, "on", json_boolean(previous_on));
            }

            continue;
        }
                
        // Setting voltage in any case, so it is just sufficient to turn on the channel
        const int voltage = json_integer_value(json_object_get(channel_config, "voltage"));

        if (global_status.verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "Channel voltage: " << voltage << "; ";
            std::cout << std::endl;
        }

        hv_card->SetChannelVoltage(channel, voltage);
        //hv_card->SetChannelTripTime(int channel, int time);
        //hv_card->SetChannelPowerDownMode(int channel, int mode);

        const bool on = json_is_true(json_object_get(channel_config, "on"));
        const bool previous_on = hv_card->IsChannelOn(channel);

        if (on)
        {
            hv_card->SetChannelOn(channel);
        }
        else
        {
            if (global_status.verbosity > 0)
            {
                std::cout << '[' << utilities_functions::time_string() << "] ";
                std::cout << "Channel OFF; ";
                std::cout << std::endl;
            }

            hv_card->SetChannelOff(channel);
        }

        if (on && !previous_on)
        {
            std::string event_message = "Channel " + std::to_string(channel) + " turned on";

            json_t *status_message = json_object();

            json_object_set_new(status_message, "type", json_string("event"));
            json_object_set_new(status_message, "event", json_string(event_message.c_str()));

            actions::generic::publish_status(global_status, defaults_hijk_events_topic, status_message);

            json_decref(status_message);
        }
        else if (!on && previous_on)
        {
            std::string event_message = "Channel " + std::to_string(channel) + " turned off";

            json_t *status_message = json_object();

            json_object_set_new(status_message, "type", json_string("event"));
            json_object_set_new(status_message, "event", json_string(event_message.c_str()));

            actions::generic::publish_status(global_status, defaults_hijk_events_topic, status_message);

            json_decref(status_message);
        }
    }

    return states::RECEIVE_COMMANDS;
}

state actions::receive_commands(status &global_status)
{
    void *commands_socket = global_status.commands_socket;

    char *buffer;
    size_t size;

    const int result = receive_byte_message(commands_socket, nullptr, (void **)(&buffer), &size, 0, global_status.verbosity);

    if (size > 0 && result == EXIT_SUCCESS)
    {
        if (global_status.verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "Message buffer: ";
            std::cout << (char*)buffer;
            std::cout << "; ";
            std::cout << std::endl;
        }

        json_error_t error;

        json_t *json_message = json_loadb(buffer, size, 0, &error);

        free(buffer);

        if (!json_message)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
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
            json_t *json_command = json_object_get(json_message, "command");
            json_t *arguments = json_object_get(json_message, "arguments");

            if (json_command != NULL)
            {
                CAENHV *hv_card = global_status.hv_card;

                if (!hv_card)
                {
                    std::cout << '[' << utilities_functions::time_string() << "] ";
                    std::cout << "ERROR: HV pointer unavailable";
                    std::cout << std::endl;

                    json_decref(json_message);
                    return states::CONFIGURE_ERROR;
                }

                const std::string command = json_string_value(json_command);

                if (command == std::string("reconfigure") && arguments != NULL)
                {
                    json_t *new_config = json_object_get(arguments, "config");

                    if (new_config != NULL)
                    {
                        global_status.config = json_deep_copy(new_config);

                        return states::CONFIGURE_HV;
                    }
                }
                else if (command == std::string("off") && arguments != NULL)
                {
                    json_t *channel = json_object_get(arguments, "channel");

                    if (channel != NULL && json_is_integer(channel))
                    {
                        const int channel_value = json_integer_value(channel);

                        if (global_status.verbosity > 0)
                        {
                            std::cout << '[' << utilities_functions::time_string() << "] ";
                            std::cout << "Turning OFF channel: " << channel_value << "; ";
                            std::cout << std::endl;
                        }

                        hv_card->SetChannelOff(channel_value);
                    }
                }
                else if (command == std::string("on") && arguments != NULL)
                {
                    json_t *channel = json_object_get(arguments, "channel");

                    if (channel != NULL && json_is_integer(channel))
                    {
                        const int channel_value = json_integer_value(channel);

                        if (global_status.verbosity > 0)
                        {
                            std::cout << '[' << utilities_functions::time_string() << "] ";
                            std::cout << "Turning ON channel: " << channel_value << "; ";
                            std::cout << std::endl;
                        }

                        hv_card->SetChannelOn(channel_value);
                    }
                }
                else if (command == std::string("set_voltage") && arguments != NULL)
                {
                    json_t *channel = json_object_get(arguments, "channel");
                    json_t *voltage = json_object_get(arguments, "voltage");

                    if (channel != NULL && json_is_integer(channel) && voltage != NULL && json_is_real(voltage))
                    {
                        const int channel_value = json_integer_value(channel);
                        const double voltage_value = json_real_value(voltage);

                        hv_card->SetChannelVoltage(channel_value, voltage_value);
                    }
                }
                else if (command == std::string("quit"))
                {
                    return states::DESTROY_HV;
                }
            }
        }
    }

    return states::PUBLISH_STATUS;
}

state actions::publish_status(status &global_status)
{
    CAENHV *hv_card = global_status.hv_card;

    json_t *status_message = json_object();

    json_object_set_new(status_message, "config", json_deep_copy(global_status.config));

    json_t *hv_card_message = json_object();

    if (hv_card)
    {
        json_object_set_new(hv_card_message, "valid_pointer", json_true());

        if (hv_card->IsActive()) {
            json_object_set_new(hv_card_message, "active", json_true());
        }
        else {
            json_object_set_new(hv_card_message, "active", json_false());
        }

        json_t *channels_message = json_array();

        const unsigned int channels = hv_card->GetChNum();

        for (unsigned int channel = 0; channel < channels; channel++)
        {
            json_t *channel_info = json_object();

            std::string status_description;

            const int *status_bits = hv_card->GetChStatusArray(channel);
            for (unsigned int i=1; i<13; i++)
            {
                if (status_bits[i] != 0)
                {
                    status_description += hv_card->GetChStatusDesc(i);
                    status_description += "; ";
                }
            }

            if (status_description.length() <= 0)
            {
                status_description = "OK";
            }

            json_object_set_new(channel_info, "id", json_integer(channel));
            json_object_set_new(channel_info, "on", hv_card->IsChannelOn(channel) ? json_true() : json_false());
            json_object_set_new(channel_info, "current", json_real(hv_card->GetChannelCurrent(channel)));
            json_object_set_new(channel_info, "voltage", json_real(hv_card->GetChannelVoltage(channel)));
            //json_object_set_new(channel_info, "temperature", json_real(hv_card->GetChannelTemperature(channel);
            json_object_set_new(channel_info, "status", json_string(status_description.c_str()));

            json_array_append_new(channels_message, channel_info);
        }

        json_object_set_new(hv_card_message, "channels", channels_message);
    }
    else
    {
        json_object_set_new(hv_card_message, "valid_pointer", json_false());
    }

    json_object_set_new(status_message, "hv_card", hv_card_message);

    actions::generic::publish_status(global_status, defaults_hijk_status_topic, status_message);

    json_decref(status_message);

    if (hv_card)
    {
        return states::RECEIVE_COMMANDS;
    }
    else
    {
        return states::CONFIGURE_ERROR;
    }
}

state actions::destroy_hv(status &global_status)
{
    json_t *status_message = json_object();

    json_object_set(status_message, "type", json_string("event"));
    json_object_set(status_message, "event", json_string("HV deactivation"));

    actions::generic::publish_status(global_status, defaults_hijk_events_topic, status_message);

    json_decref(status_message);

    json_decref(global_status.config);

    json_decref(global_status.config);

    actions::generic::destroy_hv(global_status);

    return states::CLOSE_SOCKETS;
}

state actions::close_sockets(status &global_status)
{
    void *status_socket = global_status.status_socket;
    void *commands_socket = global_status.commands_socket;

    const int s = zmq_close(status_socket);
    if (s != 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ZeroMQ Error on status socket close: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    const int c = zmq_close(commands_socket);
    if (c != 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ZeroMQ Error on commands socket close: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    return states::DESTROY_CONTEXT;
}

state actions::destroy_context(status &global_status)
{
    void *context = global_status.context;

    const int c = zmq_ctx_destroy(context);
    if (c != 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ZeroMQ Error on context destroy: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    return states::STOP;
}

state actions::communication_error(status &global_status)
{
    json_t *status_message = json_object();

    json_object_set_new(status_message, "type", json_string("error"));
    json_object_set_new(status_message, "error", json_string("Communication error"));

    actions::generic::publish_status(global_status, defaults_hijk_events_topic, status_message);

    json_decref(status_message);

    return states::CLOSE_SOCKETS;
}

state actions::configure_error(status &global_status)
{
    json_t *status_message = json_object();

    json_object_set_new(status_message, "type", json_string("error"));
    json_object_set_new(status_message, "error", json_string("Configure error"));

    actions::generic::publish_status(global_status, defaults_hijk_events_topic, status_message);

    json_decref(status_message);

    return states::RECONFIGURE_DESTROY_HV;
}

state actions::io_error(status &global_status)
{
    json_t *status_message = json_object();

    json_object_set_new(status_message, "type", json_string("error"));
    json_object_set_new(status_message, "error", json_string("I/O error"));

    actions::generic::publish_status(global_status, defaults_hijk_events_topic, status_message);

    json_decref(status_message);

    return states::CLOSE_SOCKETS;
}

state actions::stop(status&)
{
    return states::STOP;
}
