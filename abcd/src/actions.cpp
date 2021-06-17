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
#include <string>
#include <thread>
#include <zmq.h>
#include <json/json.h>

#include "class_caen_dgtz.h"

#include "defaults.h"
#include "utilities_functions.hpp"
#include "socket_functions.hpp"
#include "typedefs.hpp"
#include "events.hpp"
#include "states.hpp"
#include "actions.hpp"

/******************************************************************************/
/* Generic actions                                                            */
/******************************************************************************/

void actions::generic::publish_events(status &global_status)
{
    const size_t buffer_size = global_status.events_buffer.size();

    if (buffer_size > 0)
    {
        const size_t data_size = buffer_size * sizeof(event_PSD);

        std::string topic = defaults_abcd_data_events_topic;
        topic += "_v0";
        topic += "_n";
        topic += std::to_string(global_status.events_msg_ID);
        topic += "_s";
        topic += std::to_string(data_size);

        if (global_status.verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "Sending binary buffer; ";
            std::cout << "Topic: " << topic << "; ";
            std::cout << "events: " << buffer_size << "; ";
            std::cout << "buffer size: " << data_size << "; ";
            //std::cout << "event_PSD size: " << sizeof(event_PSD) << "; ";
            std::cout << std::endl;
        }

        const bool result = socket_functions::send_byte_message(global_status.data_socket, \
                                                                topic, \
                                                                global_status.events_buffer.data(), \
                                                                data_size);

        global_status.events_msg_ID += 1;

        if (result == false)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "ZeroMQ Error publishing events";
            std::cout << std::endl;
        }

        // Cleanup vector
        global_status.events_buffer.clear();
        // Initialize vector size to its max size plus a 10%
        global_status.events_buffer.reserve(global_status.events_buffer_max_size \
                                            + \
                                            global_status.events_buffer_max_size / 10);
    }

    const size_t waveforms_buffer_size = global_status.waveforms_buffer.size();

    if (waveforms_buffer_size > 0)
    {
        size_t total_size = 0;

        for (auto event: global_status.waveforms_buffer)
        {
            total_size += event.size();
        }

        std::vector<uint8_t> output_buffer(total_size);

        size_t portion = 0;

        for (auto event: global_status.waveforms_buffer)
        {
            const size_t event_size = event.size();
            const std::vector<uint8_t> event_buffer = event.serialize();
            
            memcpy(output_buffer.data() + portion,
                   reinterpret_cast<const void*>(event_buffer.data()),
                   event_size);

            portion += event_size;
        }

        std::string topic = defaults_abcd_data_waveforms_topic;
        topic += "_v0";
        topic += "_n";
        topic += std::to_string(global_status.waveforms_msg_ID);
        topic += "_s";
        topic += std::to_string(total_size);

        if (global_status.verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "Sending waveforms buffer; ";
            std::cout << "Topic: " << topic << "; ";
            std::cout << "waveforms: " << waveforms_buffer_size << "; ";
            std::cout << "buffer size: " << total_size << "; ";
            std::cout << std::endl;
        }

        const bool result = socket_functions::send_byte_message(global_status.data_socket, \
                                                                topic, \
                                                                output_buffer.data(), \
                                                                total_size);
        global_status.waveforms_msg_ID += 1;

        if (result == false)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "ZeroMQ Error publishing events";
            std::cout << std::endl;
        }

        // Cleanup vector
        global_status.waveforms_buffer.clear();
        // Initialize vector size to its max size plus a 10%
        global_status.waveforms_buffer.reserve(global_status.events_buffer_max_size \
                                               + \
                                               global_status.events_buffer_max_size / 10);
    }
}

void actions::generic::publish_message(status &global_status,
                                      std::string topic,
                                      Json::Value status_message)
{
    std::chrono::time_point<std::chrono::system_clock> last_publication = std::chrono::system_clock::now();
    global_status.last_publication = last_publication;

    void *status_socket = global_status.status_socket;
    const size_t status_msg_ID = global_status.status_msg_ID;

    status_message["module"] = "abcd";
    status_message["timestamp"] = utilities_functions::time_string();
    status_message["msg_ID"] = Json::Value::UInt64(status_msg_ID);

    //Json::FastWriter writer;
    //std::string message = writer.write(status_message);
    Json::StreamWriterBuilder builder;
    builder.settings_["indentation"] = "";
    std::string message = Json::writeString(builder, status_message);

    size_t total_size = message.size();

    topic += "_v0_s";
    topic += std::to_string(total_size);

    if (global_status.verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Sending status message; ";
        std::cout << "Topic: " << topic << "; ";
        std::cout << "buffer size: " << total_size << "; ";
        std::cout << "message: " << message << "; ";
        std::cout << std::endl;
    }

    void *pointer = const_cast<void*>(reinterpret_cast<const void*>(message.data()));

    socket_functions::send_byte_message(status_socket, topic, pointer, total_size);

    global_status.status_msg_ID += 1;
}

void actions::generic::stop_acquisition(status &global_status)
{
    CAENDgtz * const digitizer = global_status.digitizer;
    const unsigned int verbosity = global_status.verbosity;

    digitizer->SWStopAcquisition();

    std::chrono::time_point<std::chrono::system_clock> stop_time = std::chrono::system_clock::now();
    auto delta_time = std::chrono::duration_cast<std::chrono::duration<long int>>(stop_time - global_status.start_time);

    global_status.stop_time = stop_time;

    if (verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Run time: ";
        std::cout << delta_time.count();
        std::cout << std::endl;
    }
}

void actions::generic::clear_memory(status &global_status)
{
    CAENDgtz * const digitizer = global_status.digitizer;
    const unsigned int verbosity = global_status.verbosity;

    global_status.counts.clear();
    global_status.partial_counts.clear();
    global_status.ICR_counts.clear();
    global_status.events_buffer.clear();

    if (global_status.verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Clearing ReadoutBuffer...";
        std::cout << std::endl;
    }

    if (global_status.readout_buffer != nullptr)
    {
        global_status.digitizer->FreeReadoutBuffer(&global_status.readout_buffer);

        global_status.readout_buffer = nullptr;
    }

    if (global_status.dpp_version == 0)
    {
        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "Clearing STD Events...";
            std::cout << std::endl;
        }

        digitizer->FreeEvent((void**)(&global_status.Evt_STD));
        global_status.Evt_STD = nullptr;

        //if (global_status.Evt_STD != nullptr)
        //{
        //    if (verbosity > 0)
        //    {
        //        std::cout << '[' << utilities_functions::time_string() << "] ";
        //        std::cout << "Clearing Evt_STD...";
        //        std::cout << std::endl;
        //    }

        //    delete global_status.Evt_STD;
        //    global_status.Evt_STD = nullptr;
        //}
    }
    else if (global_status.dpp_version == 3)
    {
        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "Clearing DPP Events...";
            std::cout << std::endl;
        }

        digitizer->FreeDPPEvents((void**)global_status.Evt_PSD);

        if (global_status.enabled_waveforms)
        {
            if (verbosity > 0)
            {
                std::cout << '[' << utilities_functions::time_string() << "] ";
                std::cout << "Clearing DPP Waveforms...";
                std::cout << std::endl;
            }

            digitizer->FreeDPPWaveforms((void**)global_status.Waveforms_PSD);
        }

        if (global_status.Evt_PSD != nullptr)
        {
            if (verbosity > 0)
            {
                std::cout << '[' << utilities_functions::time_string() << "] ";
                std::cout << "Clearing Evt_PSD...";
                std::cout << std::endl;
            }

            delete global_status.Evt_PSD;
            global_status.Evt_PSD = nullptr;
        }
        //delete global_status.Waveforms_PSD;
    }
    else
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Digitizer error, firmware not supported";
        std::cout << std::endl;
    }

    if (verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Clearing numEvents...";
        std::cout << std::endl;
    }
    delete global_status.numEvents;
    global_status.numEvents = nullptr;

    //delete global_status.numSamples;

    if (verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Clearing previous_timestamp...";
        std::cout << std::endl;
    }
    delete global_status.previous_timestamp;
    global_status.previous_timestamp = nullptr;

    if (verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Clearing time_offset...";
        std::cout << std::endl;
    }
    delete global_status.time_offset;
    global_status.time_offset = nullptr;
}

void actions::generic::destroy_digitizer(status &global_status)
{
    if (global_status.digitizer)
    {
        global_status.digitizer->Deactivate();

        delete global_status.digitizer;
    }

    global_status.digitizer = nullptr;
}

bool actions::generic::create_digitizer(status &global_status)
{
    try
    {
        CAENDgtz *digitizer = new CAENDgtz;

        global_status.digitizer = digitizer;
    }
    catch (const std::bad_alloc &)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Unable to instantiate digitizer";
        std::cout << std::endl;

        return false;
    }

    return true;
}

bool actions::generic::configure_digitizer(status &global_status)
{
    CAENDgtz *digitizer = global_status.digitizer;
    unsigned int connection_type = global_status.connection_type;
    unsigned int link_number = global_status.link_number;
    unsigned int CONET_node = global_status.CONET_node;
    unsigned int VME_address = global_status.VME_address;
    unsigned int verbosity = global_status.verbosity;

    if (!digitizer)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Digitizer pointer unavailable";
        std::cout << std::endl;

        return false;
    }

    if (digitizer->IsActive() == 1)
    {
        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "Card already active, deactivating it";
            std::cout << std::endl;
        }
        digitizer->Deactivate();
    }

    if (digitizer->IsActive() == 0)
    {
        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "Activating card";
            std::cout << std::endl;
        }
        digitizer->Activate(connection_type, link_number, CONET_node, VME_address);
    }

    if (digitizer->IsActive() == 0)
    {
        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "ERROR: Unable to activate card";
            std::cout << std::endl;
        }
        digitizer->Deactivate();

        return false;
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
        digitizer->SetVerboseDebug(true);
    }
    else
    {
        digitizer->SetVerboseDebug(false);
    }

    //if (verbosity > 0)
    //{
    //    std::cout << '[' << utilities_functions::time_string() << "] ";
    //    std::cout << "Configuring with file: ";
    //    std::cout << config_file;
    //    std::cout << std::endl;
    //}
    //digitizer->ConfigureFromFile(config_file.c_str());

    digitizer->ConfigureFromJSON(global_status.config);

    if (digitizer->GetError() != 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Digitizer configuration error";
        std::cout << std::endl;

        return false;
    }

    global_status.dpp_version = digitizer->GetDPPVersion();

    // Only for DPP-PHA, DPP-PSD, DPP-CI
    // threshold: Specifies how many events to let accumulate in the board
    //            memory before they are rendered available for readout.
    //            A low number maximizes responsiveness, since data are read as
    //            soon as they are stored in memory, while a high number
    //            maximizes efficiency, since fewer transfers are made.
    //            Supplying 0 will let the library choose the best value
    //            depending on acquisition mode and other parameters.
    // maxsize:   Specifies the maximum size in bytes of the event buffer on
    //            the PC side. This parameter might be useful in case the
    //            computer has very low RAM. Normally, though, it is safe to
    //            supply 0 as this parameter, so that the library will choose an
    //            appropriate value automatically.
    if (global_status.dpp_version > 0)
    {
        digitizer->SetDPPEventAggregation(0,0);
    }

    const auto model = digitizer->GetModel();
    if (model == 730)
    {
        global_status.offset_step = ((uint64_t)1 << 47);
    }
    else
    {
        global_status.offset_step = 0x40000000;
    }

    if (verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Board name: " << digitizer->GetBoardName() << "; ";
        std::cout << "Model: " << digitizer->GetModel() << "; ";
        std::cout << "Model name: " << digitizer->GetModelName() << "; ";
        std::cout << "Firmware version: " << static_cast<int>(digitizer->GetDPPVersion()) << "; ";
        std::cout << "Number of channels: " << digitizer->GetNumChannels() << "; ";
        std::cout << "Temperature: " << digitizer->GetMaximumTemperature() << "; ";
        std::cout << std::endl;
    }

    return true;
}

bool actions::generic::allocate_memory(status &global_status)
{
    CAENDgtz *digitizer = global_status.digitizer;
    unsigned int verbosity = global_status.verbosity;

    if (!digitizer)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Digitizer pointer unavailable";
        std::cout << std::endl;

        return false;
    }

    if (verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Readout buffer memory allocation";
        std::cout << std::endl;
    }

    uint32_t bsize;

    digitizer->MallocReadoutBuffer(&global_status.readout_buffer, &bsize);

    if (verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "MallocReadoutBuffer bsize: ";
        std::cout << bsize << std::endl;
        std::cout << std::endl;
    }

    if (global_status.readout_buffer == nullptr)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Digitizer error, unable to allocate readout buffer";
        std::cout << std::endl;

        return false;
    }

    const unsigned int channels_number = digitizer->GetNumChannels();

    if (verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Channels number: ";
        std::cout << channels_number << std::endl;
        std::cout << std::endl;
    }

    global_status.counts.clear();
    global_status.counts.resize(channels_number, 0);
    global_status.partial_counts.clear();
    global_status.partial_counts.resize(channels_number, 0);
    global_status.ICR_counts.clear();
    global_status.ICR_counts.resize(channels_number, 0);

    if (verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Memory allocation";
        std::cout << std::endl;
    }

    if (global_status.dpp_version == 0)
    {
        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "STD Firmware ";
            std::cout << std::endl;
        }

        // STD firmware
        //global_status.Evt_STD = new CAEN_DGTZ_UINT16_EVENT_t *[channels_number];

        //if (global_status.Evt_STD == nullptr)
        //{
        //    std::cout << '[' << utilities_functions::time_string() << "] ";
        //    std::cout << "ERROR: Unable to allocate Evt_STD";
        //    std::cout << std::endl;

        //    return false;
        //}

        //for (unsigned int ch = 0; ch < channels_number; ch++) {
        //    global_status.Evt_STD[ch] = nullptr;
        //}

        digitizer->AllocateEvent((void**)(&global_status.Evt_STD));


    }
    else if (global_status.dpp_version == 3)
    {
        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "DPP-PSD Firmware ";
            std::cout << std::endl;
        }

        // DPP-PSD firmware
        global_status.Evt_PSD = new CAEN_DGTZ_DPP_PSD_Event_t *[MAXC_DG];

        if (global_status.Evt_PSD == nullptr)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "ERROR: Unable to allocate Evt_PSD";
            std::cout << std::endl;

            return false;
        }

        for (unsigned int ch = 0; ch < MAXC_DG; ch++) {
            global_status.Evt_PSD[ch] = nullptr;
        }

        uint32_t bsize = 0;

        digitizer->MallocDPPEvents((void**)global_status.Evt_PSD, &bsize);

        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "MallocDPPEvents bsize: ";
            std::cout << bsize << std::endl;
            std::cout << std::endl;
        }

        for (unsigned int ch = 0; ch < channels_number; ch++)
        {
            if (global_status.Evt_PSD[ch] == nullptr)
            {
                std::cout << '[' << utilities_functions::time_string() << "] ";
                std::cout << "WARNING: Digitizer error, unable to allocate DPP events buffer for channel: ";
                std::cout << ch << std::endl;
                std::cout << std::endl;

                //return false;
            }
        }

        if (digitizer->GetEnabledScope())
        {
            digitizer->MallocDPPWaveforms((void**)&global_status.Waveforms_PSD, &bsize);

            if (verbosity > 0)
            {
                std::cout << '[' << utilities_functions::time_string() << "] ";
                std::cout << "MallocDPPWaveforms bsize: ";
                std::cout << bsize << std::endl;
                std::cout << std::endl;
            }

            if (global_status.Waveforms_PSD == nullptr)
            {
                std::cout << '[' << utilities_functions::time_string() << "] ";
                std::cout << "ERROR: Digitizer error, unable to allocate DPP waveforms buffer for channel";
                std::cout << std::endl;

                return false;
            }
        }
    }
    else
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Digitizer error, firmware not supported";
        std::cout << std::endl;

        return false;
    }

    uint32_t *numEvents = new uint32_t[channels_number];
    //uint16_t *numSamples = new uint16_t[channels_number];

    for (unsigned int ch = 0; ch < channels_number; ch++) {
        numEvents[ch] = 0;
        //numSamples[ch] = digitizer->GetChannelScopeSamples(ch);
    }

    uint64_t *previous_timestamp = new uint64_t[channels_number];
    uint64_t *time_offset = new uint64_t[channels_number];

    for (unsigned int ch = 0; ch < channels_number; ch++)
    {
        previous_timestamp[ch] = 0;
        time_offset[ch] = 0;
    }

    // Reserve the events_buffer in order to have a good starting size of its buffer
    global_status.events_buffer.reserve(global_status.events_buffer_max_size \
                                        + \
                                        global_status.events_buffer_max_size / 10);

    // The first event will give the buffer size in its timestamp member
    //global_status.events_buffer.emplace_back(0, 0, 0, 0);

    // Writing to the global status all the important variables
    global_status.numEvents = numEvents;
    global_status.previous_timestamp = previous_timestamp;
    global_status.time_offset = time_offset;

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
        std::cout << '[' << utilities_functions::time_string() << "] ";
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
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: ZeroMQ Error on status socket creation: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Creates the data socket
    void *data_socket = zmq_socket(context, ZMQ_PUB);
    if (!data_socket)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: ZeroMQ Error on data socket creation: ";
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
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: ZeroMQ Error on status socket binding: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Binds the data socket to its address
    const int d = zmq_bind(global_status.data_socket, data_address.c_str());
    if (d != 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: ZeroMQ Error on data socket binding: ";
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

    std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));

    return states::READ_CONFIG;
}

state actions::read_config(status &global_status)
{
    const std::string config_file_name = global_status.config_file;

    if (global_status.verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Reading config file: " << config_file_name << " ";
        std::cout << std::endl;
    }

    std::ifstream config_file;

    config_file.open(config_file_name, std::ifstream::in);

    if (!config_file.good())
    {
        if (global_status.verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "Unable to open: " << config_file_name << " ";
            std::cout << std::endl;
        }

        return states::PARSE_ERROR;
    }

    Json::Value config;
    //Json::Reader reader;
    //const bool parse_success = reader.parse(config_file, config, false);
    Json::CharReaderBuilder json_reader;
    std::string json_parsing_error;
    const bool parse_success = Json::parseFromStream(json_reader,
                                                     config_file,
                                                     &config,
                                                     &json_parsing_error);

    config_file.close();

    if (parse_success)
    {
        global_status.config = config;

        return states::CREATE_DIGITIZER;
    }
    else
    {
        if (global_status.verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "JSON parsing error ";
            std::cout << std::endl;
        }

        return states::PARSE_ERROR;
    }
}

state actions::create_digitizer(status &global_status)
{
    Json::Value event_message;

    event_message["type"] = "event";
    event_message["event"] = "Digitizer initialization";

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, event_message);

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

    const Json::Value json_message = socket_functions::receive_json_message(commands_socket, 1);

    if (json_message.isMember("command"))
    {
        std::string command;
        try
        {
            command = json_message["command"].asString();
        }
        catch (...) { }

        if (command == std::string("start"))
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "################################################################### Start!!! ###";
            std::cout << std::endl;

            return states::START_ACQUISITION;
        }
        else if (command == std::string("reconfigure") && json_message.isMember("arguments"))
        {
            const Json::Value arguments = json_message["arguments"];

            if (arguments.isMember("config"))
            {
                global_status.config = arguments["config"];

                Json::Value event_message;

                event_message["type"] = "event";
                event_message["event"] = "Digitizer reconfiguration";

                actions::generic::publish_message(global_status, defaults_abcd_events_topic, event_message);

                return states::CONFIGURE_DIGITIZER;
            }
        }
        else if (command == std::string("off"))
        {
            return states::CLEAR_MEMORY;
        }
        else if (command == std::string("quit"))
        {
            return states::CLEAR_MEMORY;
        }
    }

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    if (now - global_status.last_publication > global_status.publish_timeout)
    {
        return states::PUBLISH_STATUS;
    }

    return states::RECEIVE_COMMANDS;
}

state actions::publish_status(status &global_status)
{
    CAENDgtz *digitizer = global_status.digitizer;

    Json::Value status_message;

    status_message["config"] = global_status.config;
    status_message["acquisition"]["running"] = false;

    if (digitizer)
    {
        status_message["digitizer"]["valid_pointer"] = true;
        status_message["digitizer"]["active"] = digitizer->IsActive() ? true : false;
    }
    else
    {
        status_message["digitizer"]["valid_pointer"] = false;
    }

    actions::generic::publish_message(global_status, defaults_abcd_status_topic, status_message);

    if (!digitizer)
    {
        return states::DIGITIZER_ERROR;
    }
    else
    {
        return states::RECEIVE_COMMANDS;
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
    Json::Value event_message;

    event_message["type"] = "event";
    event_message["event"] = "Start acquisition";

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, event_message);

    std::chrono::time_point<std::chrono::system_clock> start_time = std::chrono::system_clock::now();

    CAENDgtz * const digitizer = global_status.digitizer;

    if (!digitizer)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Digitizer pointer unavailable";
        std::cout << std::endl;

        return states::ACQUISITION_ERROR;
    }

    if (global_status.verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Clearing counters; ";
        std::cout << std::endl;
    }

    const unsigned int channels_number = digitizer->GetNumChannels();

    for (unsigned int ch = 0; ch < channels_number; ch++) {
        global_status.numEvents[ch] = 0;
        global_status.previous_timestamp[ch] = 0;
        global_status.time_offset[ch] = 0;
    }

    global_status.counts.clear();
    global_status.counts.resize(channels_number, 0);
    global_status.partial_counts.clear();
    global_status.partial_counts.resize(channels_number, 0);
    global_status.ICR_counts.clear();
    global_status.ICR_counts.resize(channels_number, 0);

    // remove when switching to V1730
    // Remove?! Setting it to 1 if 730 is detected? What?!
    global_status.flag_tt64 = (digitizer->GetEnabledBSL() == 1 \
                               && \
                               digitizer->GetModel() == 730) ? true : false;
    global_status.enabled_waveforms = digitizer->GetEnabledScope();
    global_status.show_gates = digitizer->GetShowGates();

    if (global_status.verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "DPP firmware version: ";
        std::cout << global_status.dpp_version << " ";
        std::cout << "Scope enable: ";
        std::cout << global_status.enabled_waveforms << " ";
        std::cout << "Flag extended timestamp: ";
        std::cout << global_status.flag_tt64 << " ";
        std::cout << "Offset step: ";
        std::cout << global_status.offset_step << " ";
        std::cout << std::endl;
    }

    digitizer->SWStartAcquisition();

    global_status.start_time = start_time;

    return states::ACQUISITION_RECEIVE_COMMANDS;
}

state actions::acquisition_receive_commands(status &global_status)
{
    void *commands_socket = global_status.commands_socket;

    Json::Value json_message = socket_functions::receive_json_message(commands_socket);

    if (json_message.isMember("command"))
    {
        std::string command;
        try
        {
            command = json_message["command"].asString();
        }
        catch (...) { }

        if (command == std::string("stop"))
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "#################################################################### Stop!!! ###";
            std::cout << std::endl;

            return states::STOP_PUBLISH_EVENTS;
        }
    }

    // Carlo Tintori suggests to read directly the data
    //return states::POLL_DIGITIZER;
    return states::ADD_TO_BUFFER;
}

state actions::poll_digitizer(status &global_status)
{
    CAENDgtz *digitizer = global_status.digitizer;

    if (!digitizer)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Digitizer pointer unavailable";
        std::cout << std::endl;
        return states::ACQUISITION_ERROR;
    }

    if (digitizer->IsActive() == 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Digitizer is not active";
        std::cout << std::endl;
        return states::ACQUISITION_ERROR;
    }

    CAENDgtz::acqStatus_t acq_status = digitizer->GetAcquisitionStatus();

    if (acq_status.run == 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Acquisition is not running";
        std::cout << std::endl;
        return states::ACQUISITION_ERROR;
    }
    else if (acq_status.eventReady == 1)
    {
        const unsigned int verbosity = global_status.verbosity;

        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "Available data";
            std::cout << std::endl;
        }

        return states::ADD_TO_BUFFER;
    }

    return states::ACQUISITION_PUBLISH_STATUS;
}

state actions::add_to_buffer(status &global_status)
{
    CAENDgtz * const digitizer = global_status.digitizer;

    if (!digitizer)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Digitizer pointer unavailable";
        std::cout << std::endl;
        return states::ACQUISITION_ERROR;
    }

    if (digitizer->IsActive() == 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Digitizer is not active";
        std::cout << std::endl;
        return states::ACQUISITION_ERROR;
    }

    CAENDgtz::acqStatus_t acq_status = digitizer->GetAcquisitionStatus();

    if (acq_status.run == 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Acquisition is not running";
        std::cout << std::endl;
        return states::ACQUISITION_ERROR;
    }

    uint32_t bsize = 0;

    digitizer->ReadData(CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, global_status.readout_buffer, &bsize);

    if (digitizer->GetError() != 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: ReadData error";
        std::cout << std::endl;
        // Let us continue at the moment
        //return states::ACQUISITION_ERROR;
    }

    const unsigned int verbosity = global_status.verbosity;

    if (verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Read buffer size: " << bsize;
        std::cout << std::endl;
    }

    uint64_t * const previous_timestamp = global_status.previous_timestamp;
    uint64_t * const time_offset = global_status.time_offset;

    if (bsize > 0)
    {
        // We have events...
        const unsigned int channels_number = digitizer->GetNumChannels();
        uint32_t *numEvents = global_status.numEvents;

        /**********************************************************************/
        /* 1. DATA DOWNLOAD                                                   */
        /**********************************************************************/

        if (global_status.dpp_version == 0)
        {
            if (verbosity > 0)
            {
                std::cout << '[' << utilities_functions::time_string() << "] ";
                std::cout << "STD Firmware ";
                std::cout << std::endl;
            }

            // STD firmware
            uint32_t events_number = digitizer->GetNumEvents(global_status.readout_buffer, bsize);

            if (verbosity > 0)
            {
                std::cout << '[' << utilities_functions::time_string() << "] ";
                std::cout << "events_number: " << events_number << "; ";
                std::cout << std::endl;
            }

            for (uint32_t i = 0; i < events_number; i++)
            {
                char *event_pointer = nullptr;

                const CAEN_DGTZ_EventInfo_t event_info = \
                    digitizer->GetEventInfo(global_status.readout_buffer, bsize, i, &event_pointer);

                digitizer->DecodeEvent(event_pointer, (void**)(&global_status.Evt_STD));

                // loop on channels
                for (unsigned int ch = 0; ch < channels_number; ch++)
                {
                    const uint32_t samples_number = global_status.Evt_STD->ChSize[ch];

                    if (verbosity > 1)
                    {
                        std::cout << '[' << utilities_functions::time_string() << "] ";
                        std::cout << "ChSize[" << ch << "]: " << samples_number << "; ";
                        std::cout << std::endl;
                    }

                    if (samples_number > 0)
                    {
                        // 64-bit timestamp management
                        const uint64_t trigger_time_tag = event_info.TriggerTimeTag & 0x3FFFFFFF;

                        if (trigger_time_tag < previous_timestamp[ch])
                        {
                            time_offset[ch] += global_status.offset_step;
                        }
                        previous_timestamp[ch] = trigger_time_tag;

                        const uint64_t timestamp = trigger_time_tag + time_offset[ch];
                        const uint8_t  channel = ch;

                        global_status.counts[channel] += 1;
                        global_status.partial_counts[channel] += 1;

                        global_status.waveforms_buffer.emplace_back(timestamp, channel, samples_number);

                        memcpy(global_status.waveforms_buffer.back().samples.data(), \
                            global_status.Evt_STD->DataChannel[ch], \
                            samples_number * sizeof(uint16_t));
                    }
                }
            }
        } // end of STD firmware
        else if (global_status.dpp_version == 3)
        {
            if (verbosity > 0)
            {
                std::cout << '[' << utilities_functions::time_string() << "] ";
                std::cout << "DPP-PSD Firmware ";
                std::cout << std::endl;
            }

            // DPP-PSD firmware
            digitizer->GetDPPEvents(global_status.readout_buffer, bsize, (void**)global_status.Evt_PSD, numEvents);

            // loop on channels
            for (unsigned int ch = 0; ch < channels_number; ch++)
            {
                if (verbosity > 0)
                {
                    std::cout << '[' << utilities_functions::time_string() << "] ";
                    std::cout << "numEvents[" << ch << "]: " << numEvents[ch] << "; ";
                    std::cout << std::endl;
                }

                // loop on events
                for (uint32_t i = 0; i < numEvents[ch]; i++)
                {
                    // reject events with small, negative energy (mostly noise peaks)
                    // Negative but in an unsigned int, so buffer underflows (?)
                    //if ((uint16_t)global_status.Evt_PSD[ch][i].ChargeLong > defaults_abcd_negative_limit \
                    //    || \
                    //    (uint16_t)global_status.Evt_PSD[ch][i].ChargeShort > defaults_abcd_negative_limit)
                    //{
                    //    continue;
                    //}

                    // reject low charge events
                    //if ((uint16_t)global_status.Evt_PSD[ch][i].ChargeLong < spectrumThr[ch])
                    //{
                    //    continue;
                    //}

                    // Reading the ICR flag from Extra word in the DPP firmware
                    // When the flag is seen the digitizer's input counter saw 1024 events
                    if ((((uint64_t)global_status.Evt_PSD[ch][i].Extras) >> 13 ) & 1)
                    {
                        global_status.ICR_counts[ch] += 1024;
                    }

                    // 64-bit timestamp management
                    uint64_t timestamp64bit;
                    uint64_t fine_timestamp = 0;

                    if (global_status.flag_tt64)
                    {
                        timestamp64bit = (global_status.Evt_PSD[ch][i].TimeTag & 0x7FFFFFFF) \
                                    | \
                                    ((((uint64_t)global_status.Evt_PSD[ch][i].Extras) & 0xFFFF0000) << 15 );

                        fine_timestamp = global_status.Evt_PSD[ch][i].Extras & 0x3FF;

                        // corrections for isolated 4 s jumps in future
                        // LSB of Extras (bit 31 of 64 bit timestamp) flips to 1 (shark peak)
                        if ((previous_timestamp[ch] + ((uint64_t)(1 << 30)) < timestamp64bit) \
                            && \
                            (i + 1 < numEvents[ch]))
                        {
                            // jump greater than 4 seconds && we have following event to check
                            uint64_t nexttag64bit = (global_status.Evt_PSD[ch][i+1].TimeTag & 0x7FFFFFFF) \
                                                    | \
                                                    ((((uint64_t)global_status.Evt_PSD[ch][i + 1].Extras) & 0xFFFF0000) << 15 );
                            if (nexttag64bit + ((uint64_t)1 << 30) < timestamp64bit)
                            {
                                timestamp64bit -= ((uint64_t)1 << 31);
                            }
                        }

                        // This is to check if there is a jump of more than 4.3 s to the past
                        if (timestamp64bit + ((uint64_t)1 << 31) < previous_timestamp[ch])
                        {
                            time_offset[ch] += ((uint64_t)1 << 47);
                        }
                    }
                    else
                    {
                        // x720
                        timestamp64bit = (global_status.Evt_PSD[ch][i].TimeTag & 0x3FFFFFFF);

                        if (timestamp64bit < previous_timestamp[ch])
                        {
                            time_offset[ch] += 0x40000000;
                        }
                    } // end if for 64-bit timestamp

                    previous_timestamp[ch] = timestamp64bit;
                    // end 64-bit timestamp management

                    const uint64_t temporary_timestamp = (previous_timestamp[ch] + time_offset[ch]);

                    // FIXME: Testing the fine time tag from the V1730 DCFD
                    // If we shift the timestamp by 10 bits we should be able to fit the fine time tag
                    // that we get from the DCFD of the v1730s
                    const uint64_t timestamp = (temporary_timestamp << 10) + fine_timestamp;
                    
                    const uint16_t qshort = 0 + global_status.Evt_PSD[ch][i].ChargeShort;
                    const uint16_t qlong  = 0 + global_status.Evt_PSD[ch][i].ChargeLong;
                    //const uint16_t baseline = 0 + (global_status.Evt_PSD[ch][i].Extras & 0x0000FFFF);
                    const uint16_t baseline = global_status.Evt_PSD[ch][i].Baseline;
                    const uint8_t  channel = ch;
                    const uint8_t  pur = global_status.Evt_PSD[ch][i].Pur;

                    //std::cerr << "MAIN: Event: timestamp: " << timestamp << std::endl;
                    //std::cerr << "MAIN: Event: baseline: " << baseline << std::endl;
                    //std::cerr << "MAIN: Event: qshort: " << qshort << std::endl;
                    //std::cerr << "MAIN: Event: qlong: " << qlong << std::endl;
                    //std::cerr << "MAIN: Event: extras: " << global_status.Evt_PSD[ch][i].Extras << std::endl;

                    global_status.events_buffer.emplace_back(timestamp, qshort, qlong, baseline, channel, pur);

                    global_status.counts[channel] += 1;
                    global_status.partial_counts[channel] += 1;

                    if (verbosity > 1 && (i % 1000 == 0))
                    {
                        std::cout << '[' << utilities_functions::time_string() << "] ";
                        std::cout << "Event[" << i << "]: ";
                        std::cout << "timestamp: " << timestamp << "; ";
                        std::cout << "qshort: " << qshort << "; ";
                        std::cout << "qlong: " << qlong << "; ";
                        std::cout << std::endl;
                    }

                    if (global_status.enabled_waveforms)
                    {
                        digitizer->DecodeDPPWaveforms(&global_status.Evt_PSD[ch][i], global_status.Waveforms_PSD);

                        const uint32_t samples_number = global_status.Waveforms_PSD->Ns;

                        //std::cerr << "MAIN: Event: Number of samples: " << global_status.Waveforms_PSD->Ns << std::endl;

                        if (global_status.show_gates)
                        {
                            global_status.waveforms_buffer.emplace_back(timestamp, channel, samples_number, 4);

                            memcpy(global_status.waveforms_buffer.back().samples.data(), \
                                   global_status.Waveforms_PSD->Trace1, \
                                   samples_number * sizeof(uint16_t));

                            // Reading the gates waveforms
                            memcpy(global_status.waveforms_buffer.back().additional_samples[0].data(),
                                global_status.Waveforms_PSD->DTrace1,
                                samples_number * sizeof(uint8_t));
                            memcpy(global_status.waveforms_buffer.back().additional_samples[1].data(),
                                global_status.Waveforms_PSD->DTrace2,
                                samples_number * sizeof(uint8_t));
                            memcpy(global_status.waveforms_buffer.back().additional_samples[2].data(),
                                global_status.Waveforms_PSD->DTrace3,
                                samples_number * sizeof(uint8_t));
                            memcpy(global_status.waveforms_buffer.back().additional_samples[3].data(),
                                global_status.Waveforms_PSD->DTrace4,
                                samples_number * sizeof(uint8_t));
                        }
                        else
                        {
                            global_status.waveforms_buffer.emplace_back(timestamp, channel, samples_number);

                            memcpy(global_status.waveforms_buffer.back().samples.data(),
                                   global_status.Waveforms_PSD->Trace1,
                                   samples_number * sizeof(uint16_t));
                        }
                    }
                } // end loop on events
            } // end loop on channels
        } // end of DPP-PSD firmware

        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "Events buffer size: " << global_status.events_buffer.size() << "; ";
            std::cout << "Waveforms buffer size: " << global_status.waveforms_buffer.size() << "; ";
            std::cout << std::endl;
        }

        if (global_status.events_buffer.size() >= global_status.events_buffer_max_size \
            || \
            global_status.waveforms_buffer.size() >= global_status.events_buffer_max_size)
        {
            return states::PUBLISH_EVENTS;
        }
    }

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    if (now - global_status.last_publication > global_status.publish_timeout)
    {
        return states::PUBLISH_EVENTS;
    }

    return states::ADD_TO_BUFFER;
}

state actions::publish_events(status &global_status)
{
    actions::generic::publish_events(global_status);

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    if (now - global_status.last_publication > global_status.publish_timeout)
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
    CAENDgtz *digitizer = global_status.digitizer;

    Json::Value status_message;

    status_message["config"] = global_status.config;

    if (digitizer)
    {
        status_message["digitizer"]["valid_pointer"] = true;
        status_message["digitizer"]["active"] = digitizer->IsActive() ? true : false;
        status_message["acquisition"]["running"] = true;

        const auto now = std::chrono::system_clock::now();

        const auto run_delta_time = std::chrono::duration_cast<std::chrono::duration<long int>>(now - global_status.start_time);
        const long int runtime = run_delta_time.count();

        status_message["acquisition"]["runtime"] = Json::Value::UInt64(runtime);

        const auto pub_delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(now - global_status.last_publication);
        const double pubtime = static_cast<double>(pub_delta_time.count());

        status_message["acquisition"]["rates"] = Json::Value(Json::ValueType::arrayValue);
        status_message["acquisition"]["ICR_rates"] = Json::Value(Json::ValueType::arrayValue);

        if (pubtime > 0)
        {
            for (unsigned int i = 0; i < global_status.partial_counts.size(); i++)
            {
                const double rate = global_status.partial_counts[i] / pubtime * 1000;
                status_message["acquisition"]["rates"].append(rate);
            }
            for (unsigned int i = 0; i < global_status.partial_counts.size(); i++)
            {
                const double rate = global_status.ICR_counts[i] / pubtime * 1000;
                status_message["acquisition"]["ICR_rates"].append(rate);
            }
        }
        else
        {
            for (unsigned int i = 0; i < global_status.partial_counts.size(); i++)
            {
                status_message["acquisition"]["rates"].append(0);
                status_message["acquisition"]["ICR_rates"].append(0);
            }
        }

        status_message["acquisition"]["counts"] = Json::Value(Json::ValueType::arrayValue);
        status_message["acquisition"]["ICR_counts"] = Json::Value(Json::ValueType::arrayValue);

        for (unsigned int i = 0; i < global_status.counts.size(); i++)
        {
            const unsigned int channel_counts = global_status.counts[i];
            status_message["acquisition"]["counts"].append(channel_counts);

            const unsigned int channel_ICR_counts = global_status.ICR_counts[i];
            status_message["acquisition"]["ICR_counts"].append(channel_ICR_counts);
        }
    }
    else
    {
        status_message["digitizer"]["valid_pointer"] = false;
        status_message["acquisition"]["running"] = false;
    }

    // Clear event partial counts
    for (unsigned int i = 0; i < global_status.partial_counts.size(); i++)
    {
        global_status.partial_counts[i] = 0;
    }
    for (unsigned int i = 0; i < global_status.ICR_counts.size(); i++)
    {
        global_status.ICR_counts[i] = 0;
    }


    actions::generic::publish_message(global_status, defaults_abcd_status_topic, status_message);

    if (digitizer)
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

    Json::Value event_message;

    event_message["type"] = "event";
    event_message["event"] = "Stop acquisition (duration: " + std::to_string(delta_time.count()) + " s)";

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, event_message);

    return states::RECEIVE_COMMANDS;
}

state actions::clear_memory(status &global_status)
{
    if (global_status.digitizer)
    {
        actions::generic::clear_memory(global_status);
    }
    else
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Digitizer pointer unavailable";
        std::cout << std::endl;
    }

    return states::DESTROY_DIGITIZER;
}

state actions::reconfigure_clear_memory(status &global_status)
{
    if (global_status.digitizer)
    {
        actions::generic::clear_memory(global_status);
    }
    else
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ERROR: Digitizer pointer unavailable";
        std::cout << std::endl;

        return states::CONFIGURE_ERROR;
    }

    return states::RECONFIGURE_DESTROY_DIGITIZER;
}



state actions::destroy_digitizer(status &global_status)
{
    Json::Value event_message;

    event_message["type"] = "event";
    event_message["event"] = "Digitizer deactivation";

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, event_message);

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

    Json::Value event_message;

    event_message["type"] = "event";
    event_message["event"] = "Restart stop acquisition";

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, event_message);

    return states::RESTART_CLEAR_MEMORY;
}

state actions::restart_clear_memory(status &global_status)
{
    if (global_status.digitizer)
    {
        actions::generic::clear_memory(global_status);
    }
    else
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
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
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ZeroMQ Error on status socket close: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    const int d = zmq_close(data_socket);
    if (d != 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "ZeroMQ Error on data socket close: ";
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
    std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));

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
    Json::Value event_message;

    event_message["type"] = "error";
    event_message["error"] = "Communication error";

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, event_message);

    return states::CLOSE_SOCKETS;
}

state actions::parse_error(status &global_status)
{
    Json::Value event_message;

    event_message["type"] = "error";
    event_message["error"] = "Config parse error";

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, event_message);

    return states::CLOSE_SOCKETS;
}

state actions::configure_error(status &global_status)
{
    Json::Value event_message;

    event_message["type"] = "error";
    event_message["error"] = "Configure error";

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, event_message);

    return states::RECONFIGURE_DESTROY_DIGITIZER;
}

state actions::digitizer_error(status &global_status)
{
    Json::Value event_message;

    event_message["type"] = "error";
    event_message["error"] = "Digitizer error";

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, event_message);

    return states::RECONFIGURE_CLEAR_MEMORY;
}

state actions::acquisition_error(status &global_status)
{
    Json::Value event_message;

    event_message["type"] = "error";
    event_message["error"] = "Acquisition error";

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, event_message);

    return states::RESTART_PUBLISH_EVENTS;
}

state actions::restart_configure_error(status &global_status)
{
    Json::Value event_message;

    event_message["type"] = "error";
    event_message["error"] = "Restart configure error";

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, event_message);

    return states::RESTART_DESTROY_DIGITIZER;
}

