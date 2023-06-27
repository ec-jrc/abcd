/*
 * (C) Copyright 2022, European Union, Cristiano Lino Fontana
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
#include <vector>
#include <map>
// For std::pair
#include <utility>
#include <cmath>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <zmq.h>

// For system()
#include <stdlib.h>
// For nanosleep()
#include <ctime>

#include <jansson.h>

extern "C" {
#include "defaults.h"
#include "utilities_functions.h"
#include "socket_functions.h"
#include "jansson_socket_functions.h"
#include "events.h"
}

#include "typedefs.hpp"
#include "states.hpp"
#include "actions.hpp"

// The ADQAPI needs this macro, otherwise it assumes that it is running in windows
#define LINUX
#include "ADQAPI.h"
#include "ADQ_utilities.hpp"
#include "ADQ_descriptions.hpp"

#include "Digitizer.hpp"
#include "ADQ412.hpp"
#include "ADQ214.hpp"
#include "ADQ14_FWDAQ.hpp"
#include "ADQ14_FWPD.hpp"

#define WRITE_RED "\033[0;31m"
#define WRITE_YELLOW "\033[0;33m"
#define WRITE_NC "\033[0m"

#define BUFFER_SIZE 32

// The global channel number of each card is calculated with the formula:
// global_channel_number = board_channel_number + board_user_id * GetChannelsNumber()

/******************************************************************************/
/* Generic actions                                                            */
/******************************************************************************/

void actions::generic::publish_events(status &global_status)
{
    const size_t waveforms_buffer_size_Bytes = global_status.waveforms_buffer.size();

    if (waveforms_buffer_size_Bytes > 0)
    {
        std::string topic = defaults_abcd_data_waveforms_topic;
        topic += "_v0_s";
        topic += std::to_string((long long unsigned int)waveforms_buffer_size_Bytes);

        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Sending waveforms buffer; ";
            std::cout << "Topic: " << topic << "; ";
            std::cout << "waveforms: " << waveforms_buffer_size_Bytes << "; ";
            std::cout << std::endl;
        }

        const bool result = send_byte_message(global_status.data_socket,
                                              topic.c_str(),
                                              global_status.waveforms_buffer.data(),
                                              waveforms_buffer_size_Bytes,
                                              0);

        if (result == EXIT_FAILURE)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR: ZeroMQ Error publishing events";
            std::cout << std::endl;
        }

        global_status.waveforms_buffer_size_Number = 0;

        // Cleanup vector
        global_status.waveforms_buffer.clear();
        // Initialize vector size to its last size
        global_status.waveforms_buffer.reserve(waveforms_buffer_size_Bytes);
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
        topic += std::to_string((long long unsigned int)total_size);

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

        send_byte_message(status_socket, topic.c_str(), output_buffer, total_size, 0);

        free(output_buffer);
    }

    global_status.status_msg_ID += 1;
}

int actions::generic::start_acquisition(status &global_status, unsigned int digitizer_index)
{
    const unsigned int verbosity = global_status.verbosity;

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "#### Starting acquisition!!! ";
        std::cout << std::endl;
    }

    const unsigned int user_id = global_status.digitizers_user_ids.at(digitizer_index);
    auto digitizer = global_status.digitizers[digitizer_index];

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Starting acquisition of card id: " << user_id << "; ";
        std::cout << "name: " << digitizer->GetName() << "; ";
        std::cout << std::endl;
    }

    return digitizer->StartAcquisition();
}

void actions::generic::rearm_trigger(status &global_status, unsigned int digitizer_index)
{
    const unsigned int verbosity = global_status.verbosity;

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "#### Rearming trigger!!! ";
        std::cout << std::endl;
    }

    const unsigned int user_id = global_status.digitizers_user_ids.at(digitizer_index);
    auto digitizer = global_status.digitizers[digitizer_index];

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Arming trigger of card: id: " << user_id << "; ";
        std::cout << "name: " << digitizer->GetName() << "; ";
        std::cout << std::endl;
    }

    digitizer->RearmTrigger();
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

    for (auto value = global_status.digitizers_user_ids.begin(); value != global_status.digitizers_user_ids.end(); ++value)
    {
        const unsigned int digitizer_index = value->first;
        const unsigned int user_id = value->second;
        auto digitizer = global_status.digitizers[digitizer_index];

        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Arming trigger of card: id: " << user_id << "; ";
            std::cout << "name: " << digitizer->GetName() << "; ";
            std::cout << std::endl;
        }

        digitizer->StopAcquisition();
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
    global_status.waveforms_buffer_size_Number = 0;

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
        std::cout << "Destroying digitizers; ";
        std::cout << std::endl;
    }

    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string("Digitizers deactivation"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    for (unsigned int index = 0; index < global_status.digitizers.size(); index++)
    {
        ABCD::Digitizer *digitizer = global_status.digitizers[index];

        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Destroying card: " << digitizer->GetName() << "; ";
            std::cout << std::endl;
        }

        delete digitizer;
    }

    global_status.digitizers.clear();


    // TODO: Reset USB3 devices with ADQControlUnit_ResetDevice() and reset level 18
}

bool actions::generic::create_digitizer(status &global_status)
{
    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Initializing digitizers; ";
        std::cout << std::endl;
    }

    //const int number_of_found_devices = ADQControlUnit_FindDevices(global_status.adq_cu_ptr);

    //const int number_of_ADQs = ADQControlUnit_NofADQ(global_status.adq_cu_ptr);
    //const int number_of_ADQ412 = ADQControlUnit_NofADQ412(global_status.adq_cu_ptr);
    //const int number_of_ADQ214 = ADQControlUnit_NofADQ214(global_status.adq_cu_ptr);
    //const int number_of_ADQ14 = ADQControlUnit_NofADQ14(global_status.adq_cu_ptr);

    struct ADQInfoListEntry* ADQlist;
    unsigned int number_of_devices = 256;

    if(!ADQControlUnit_ListDevices(global_status.adq_cu_ptr, &ADQlist, &number_of_devices))
    {
        const unsigned int error_code = ADQControlUnit_GetLastFailedDeviceError(global_status.adq_cu_ptr);
        if (error_code == 0x00000001)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": ADQAPI error: " << error_code << ", ";
            std::cout << "  description: " << WRITE_RED << "ListDevices failed! The linked ADQAPI is not for the correct OS, please select correct x86/x64 platform when building." << WRITE_NC << "; ";
            std::cout << std::endl;
        }
    }

    unsigned int number_of_ADQ214 = 0;
    unsigned int number_of_ADQ412 = 0;
    unsigned int number_of_ADQ14 = 0;
    unsigned int number_of_other = 0;

    for (unsigned int device_index = 0; device_index < number_of_devices; device_index++) {
        switch (ADQlist[device_index].ProductID) {
            case PID_ADQ214:
                number_of_ADQ214 += 1;
                break;
            case PID_ADQ412:
                number_of_ADQ412 += 1;
                break;
            case PID_ADQ14:
                number_of_ADQ14 += 1;
                break;
            default:
                number_of_other += 1;
        }
    }

    const unsigned int number_of_ADQs = number_of_ADQ214 + number_of_ADQ412 + number_of_ADQ14;

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Number of devices: " << number_of_devices << "; ";
        std::cout << "Number of ADQs: " << number_of_ADQs << "; ";
        std::cout << "Number of ADQ214: " << number_of_ADQ214 << "; ";
        std::cout << "Number of ADQ412: " << number_of_ADQ412 << "; ";
        std::cout << "Number of ADQ14: " << number_of_ADQ14 << "; ";
        std::cout << "Number of other devices: " << number_of_other << "; ";
        std::cout << std::endl;
    }

    std::string initialization_string = "Digitizers initialization: ";
    initialization_string += "Number of ADQs: " + std::to_string(number_of_ADQs) + "; ";

    if (number_of_ADQ412 > 0) {
        initialization_string += "Number of ADQ412: " + std::to_string(number_of_ADQ412) + "; ";
    }
    if (number_of_ADQ214 > 0) {
        initialization_string += "Number of ADQ214: " + std::to_string(number_of_ADQ214) + "; ";
    }
    if (number_of_ADQ14 > 0) {
        initialization_string += "Number of ADQ14: " + std::to_string(number_of_ADQ14) + "; ";
    }

    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string(initialization_string.c_str()));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    for (unsigned int device_index = 0; device_index < number_of_devices; device_index++) {
        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Device index: " << device_index << "; ";
            std::cout << std::endl;
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Product id: " << ADQlist[device_index].ProductID << "; ";
            std::cout << std::endl;
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Address: "  << ADQlist[device_index].AddressField1 << " " << ADQlist[device_index].AddressField2 << "; ";
            std::cout << std::endl;
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Device file: "  << ADQlist[device_index].DevFile << "; ";
            std::cout << std::endl;
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Opened interface: "  << ADQlist[device_index].DeviceInterfaceOpened << "; ";
            std::cout << std::endl;
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Set-up completed: "  << ADQlist[device_index].DeviceSetupCompleted << "; ";
            std::cout << std::endl;
        }

        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Opening device interface establishing a communication channel for device: " << device_index << "; ";
            std::cout << std::endl;
        }

        CHECKZERO(ADQControlUnit_OpenDeviceInterface(global_status.adq_cu_ptr, device_index));

        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Setting-up device: " << device_index << "; ";
            std::cout << std::endl;
        }

        CHECKZERO(ADQControlUnit_SetupDevice(global_status.adq_cu_ptr, device_index));

        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Information about device: " << device_index << "; ";
            std::cout << std::endl;
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ADQ type: " << ADQ_GetADQType(global_status.adq_cu_ptr, device_index + 1) << "; ";
            std::cout << "Board product name: " << ADQ_GetBoardProductName(global_status.adq_cu_ptr, device_index + 1) << "; ";
            // This is not supported for all cards, and it might generate a
            // segfault since it returns a string that might not have been
            // initialized.
            //std::cout << "Card option: " << ADQ_GetCardOption(global_status.adq_cu_ptr, device_index + 1) << "; ";
            std::cout << std::endl;

            unsigned int family = 0;
            CHECKZERO(ADQ_GetProductFamily(global_status.adq_cu_ptr, device_index + 1, &family));

            std::cout << '[' << time_buffer << "] ";
            std::cout << "Product ID: " << ADQ_GetProductID(global_status.adq_cu_ptr, device_index + 1) << "; ";
            std::cout << "Product family: " << family << "; ";
            std::cout << std::endl;

            uint32_t *revision = ADQ_GetRevision(global_status.adq_cu_ptr, device_index + 1);

            std::cout << '[' << time_buffer << "] ";
            std::cout << "Revision: {";
            for (int i = 0; i < 6; i++) {
                std::cout << (unsigned int)revision[i] << ", ";
            }
            std::cout << "}; ";
            std::cout << "Firmware type: ";
            try {
                std::cout << ADQ_descriptions::ADQ_firmware_revisions.at(revision[0]);
            } catch (const std::out_of_range& error) {
                std::cout << "unknown";
            }

            std::cout << "; ";
            std::cout << std::endl;

            std::cout << '[' << time_buffer << "] ";
            std::cout << "Serial number: " << ADQ_GetBoardSerialNumber(global_status.adq_cu_ptr, device_index + 1) << "; ";
            std::cout << std::endl;
        }

        if (ADQlist[device_index].ProductID == PID_ADQ214) {
            // WARNING: boards numbering start from 1 in the next functions
            const int adq214_index = device_index + 1;

            ABCD::ADQ214 *adq214_ptr = new ABCD::ADQ214(global_status.adq_cu_ptr, adq214_index, global_status.verbosity);

            adq214_ptr->Initialize();

            global_status.digitizers.push_back(adq214_ptr);
        } else if (ADQlist[device_index].ProductID == PID_ADQ412) {
            // WARNING: boards numbering start from 1 in the next functions
            const int adq412_index = device_index + 1;

            ABCD::ADQ412 *adq412_ptr = new ABCD::ADQ412(global_status.adq_cu_ptr, adq412_index, global_status.verbosity);

            adq412_ptr->Initialize();

            global_status.digitizers.push_back(adq412_ptr);
        } else if (ADQlist[device_index].ProductID == PID_ADQ14) {
            // WARNING: boards numbering start from 1 in the next functions
            const int adq14_index = device_index + 1;

            // Check that the Pulse Detect firmware is enabled on this device.
            const int has_FWPD = ADQ_HasFeature(global_status.adq_cu_ptr, adq14_index, "FWPD");

            // These are just for information
            if (has_FWPD == 1) {
                if (global_status.verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ADQ14 (index " << adq14_index << ") Has the Pulse detect firmware; ";
                    std::cout << std::endl;
                }
            } else if (has_FWPD == -1) {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << WRITE_YELLOW << "WARNING" << WRITE_NC << ": ADQ14 (index " << adq14_index << ") does not have the Pulse Detect firmware; ";
                std::cout << std::endl;
            } else if (has_FWPD == 0) {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << WRITE_YELLOW << "WARNING" << WRITE_NC << ": ADQ14 (index " << adq14_index << ") did not respond to the firmware request; ";
                std::cout << std::endl;
            } else {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": ADQ14 (index " << adq14_index << ") unexpected value from the firmware request: " << has_FWPD << "; ";
                std::cout << std::endl;
            }

            // FIXME: This might be more reliable with older cards, but we would
            //        need to know all the firmware versions
            //int *revision = ADQ_GetRevision(global_status.adq_cu_ptr, device_index + 1);
            //if (ADQ_descriptions::ADQ_firmware_revisions.at(revision[0]) == "ADQ14_FWPD")

            if (has_FWPD) {
                ABCD::ADQ14_FWPD *adq14_ptr = new ABCD::ADQ14_FWPD(global_status.adq_cu_ptr, adq14_index, global_status.verbosity);

                adq14_ptr->Initialize();

                global_status.digitizers.push_back(adq14_ptr);


            } else {
                ABCD::ADQ14_FWDAQ *adq14_ptr = new ABCD::ADQ14_FWDAQ(global_status.adq_cu_ptr, adq14_index, global_status.verbosity);

                adq14_ptr->Initialize();

                global_status.digitizers.push_back(adq14_ptr);
            }
        }
    }

    const int number_of_failed_devices = ADQControlUnit_GetFailedDeviceCount(global_status.adq_cu_ptr);

    if (number_of_failed_devices > 0) {
        char error_cstring[512];
        unsigned int error_code = ADQControlUnit_GetLastFailedDeviceErrorWithText(global_status.adq_cu_ptr, error_cstring);

        ADQControlUnit_ClearLastFailedDeviceError(global_status.adq_cu_ptr);

        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": ADQAPI error: " << error_code << ", ";
        std::cout << "  description: " << WRITE_RED << error_cstring << WRITE_NC << "; ";
        std::cout << std::endl;

        std::string error_string("ADQAPI error: ");
        error_string += std::to_string(error_code);
        error_string += ", description: ";
        error_string += error_cstring;


        json_t *json_event_message = json_object();

        json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
        json_object_set_new_nocheck(json_event_message, "error", json_string(error_string.c_str()));

        actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

        json_decref(json_event_message);
    }

    return true;
}

bool actions::generic::configure_digitizer(status &global_status)
{
    unsigned int verbosity = global_status.verbosity;

    if (global_status.verbosity > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Configuring digitizer; ";
        std::cout << std::endl;
    }

    global_status.digitizers_user_ids.clear();
    global_status.user_scripts.clear();

    // -------------------------------------------------------------------------
    //  Starting the global configuration
    // -------------------------------------------------------------------------
    json_t *config = global_status.config;

    json_t *json_global = json_object_get(config, "global");

    if (!json_global)
    {
        json_object_set_new_nocheck(config, "global", json_object());
        json_global = json_object_get(config, "global");

        if (verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << WRITE_YELLOW << "WARNING" << WRITE_NC << ": Missing \"global\" entry in configuration; ";
            std::cout << std::endl;
        }


    }

    if (json_global != NULL && json_is_object(json_global))
    {
        // WARNING: The key has a different spelling for the user convenience
        int waveforms_buffer_size_max_Number = json_number_value(json_object_get(json_global, "waveforms_buffer_max_size"));

        if (waveforms_buffer_size_max_Number <= 0) {
            waveforms_buffer_size_max_Number = defaults_absp_waveforms_buffer_size_max_Number;
        }

        if (verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Waveforms buffer max size: " << waveforms_buffer_size_max_Number << "; ";
            std::cout << std::endl;
        }

        global_status.waveforms_buffer_size_max_Number = waveforms_buffer_size_max_Number;

        json_object_set_new_nocheck(json_global, "waveforms_buffer_max_size", json_integer(waveforms_buffer_size_max_Number));
    }

    unsigned int max_channel_number = 0;

    json_t *json_cards = json_object_get(config, "cards");

    if (json_cards != NULL && json_is_array(json_cards))
    {
        size_t index;
        json_t *card;

        json_array_foreach(json_cards, index, card) {
            const unsigned int user_id = json_integer_value(json_object_get(card, "id"));
            const char *cstr_serial = json_string_value(json_object_get(card, "serial"));
            const std::string serial = (cstr_serial) ? std::string(cstr_serial) : std::string();

            if (verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Found card: " << serial << "; ";
                std::cout << "user id: " << user_id << "; ";
                std::cout << std::endl;
            }

            const bool enabled = json_is_true(json_object_get(card, "enable"));

            if (verbosity > 0 && enabled)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Card is enabled; ";
                std::cout << std::endl;
            }
            else if (verbosity > 0 && !enabled)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Card is disabled; ";
                std::cout << std::endl;
            }

            for (unsigned int digitizer_index = 0; digitizer_index < global_status.digitizers.size() && enabled; digitizer_index++)
            {
                auto digitizer = global_status.digitizers[digitizer_index];

                if ((digitizer)->GetName() == serial) {
                    if (verbosity > 0)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "Found matching card: " << serial << "; ";
                        std::cout << std::endl;
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "Parsing configuration; ";
                        std::cout << std::endl;
                    }

                    (digitizer)->ReadConfig(card);

                    if (verbosity > 0)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "Applying configuration; ";
                        std::cout << std::endl;
                    }

                    (digitizer)->Configure();

                    if (verbosity > 0)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "Finished configuration; ";
                        std::cout << std::endl;
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "digitizer_index: " << digitizer_index << "; ";
                        std::cout << "user id: " << user_id << "; ";
                        std::cout << std::endl;
                    }

                    const unsigned int this_max_channel_number = (user_id + 1) * (digitizer)->GetChannelsNumber();

                    if (max_channel_number < this_max_channel_number) {
                        max_channel_number = this_max_channel_number;
                    }

                    if (verbosity > 0)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "Channels number: " << digitizer->GetChannelsNumber() << "; ";
                        std::cout << "Max channel number: " << this_max_channel_number << "; ";
                        std::cout << std::endl;
                    }

                    global_status.digitizers_user_ids[digitizer_index] = user_id;

                    std::string configure_string = "Digitizer configuration, model: ";
                    configure_string += digitizer->GetModel();
                    configure_string += ", serial: ";
                    configure_string += digitizer->GetName();
                    configure_string += ", user id: ";
                    configure_string += std::to_string(user_id);

                    json_t *json_event_message = json_object();

                    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
                    json_object_set_new_nocheck(json_event_message, "event", json_string(configure_string.c_str()));

                    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

                    json_decref(json_event_message);

                    break;
                }
            }
        }
    }

    global_status.channels_number = max_channel_number;

    if (verbosity > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Channels number: " << max_channel_number << "; ";
        std::cout << std::endl;
    }

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Digitizers configuration completed successfully!";
        std::cout << std::endl;
    }

    json_t *json_scripts = json_object_get(config, "scripts");

    if (json_scripts != NULL && json_is_array(json_scripts))
    {
        size_t index;
        json_t *json_script;

        json_array_foreach(json_scripts, index, json_script) {
            const bool enabled = json_is_true(json_object_get(json_script, "enable"));

            if (verbosity > 0 && enabled)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Script is enabled; ";
                std::cout << std::endl;
            }
            else if (verbosity > 0 && !enabled)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Script is disabled; ";
                std::cout << std::endl;
            }

            if (enabled) {
                // The state may be a single integer or an array of integers
                json_t *json_state = json_object_get(json_script, "state");

                std::vector<unsigned int> script_states;

                if (json_state != NULL && json_is_integer(json_state)) {
                    const unsigned int state = json_number_value(json_state);
                    script_states.push_back(state);
                } else if (json_state != NULL && json_is_array(json_state)) {
                    size_t state_index;
                    json_t *state_value;

                    json_array_foreach(json_state, state_index, state_value) {
                        if (state_value != NULL && json_is_integer(state_value)) {
                            const unsigned int state = json_number_value(state_value);
                            script_states.push_back(state);
                        }
                    }
                }

                const char *cstr_when = json_string_value(json_object_get(json_script, "when"));
                const std::string str_when = (cstr_when) ? std::string(cstr_when) : std::string();
                int when = SCRIPT_WHEN_POST;

                if (str_when == "pre") {
                    when = SCRIPT_WHEN_PRE;
                } else if (str_when == "post") {
                    when = SCRIPT_WHEN_POST;
                } else {
                    when = SCRIPT_WHEN_POST;
                }

                const char *cstr_source = json_string_value(json_object_get(json_script, "source"));
                std::string str_source = (cstr_source) ? std::string(cstr_source) : std::string();

                std::ifstream source_file(str_source);

                // If the source entry contains the name of an existing file, then
                // we read the content of the file in the string.
                if (source_file.good()) {
                    if (verbosity > 0)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "The source of this script is in the file: " << str_source << "; ";
                        std::cout << std::endl;
                    }

                    std::stringstream buffer;
                    buffer << source_file.rdbuf();

                    str_source = buffer.str();
                } else {
                    if (verbosity > 0)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "The source of this script is: " << str_source << "; ";
                        std::cout << std::endl;
                    }
                }

                for (const auto& script_state : script_states) {
                    if (verbosity > 0)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "Found script for state: " << script_state << "; ";
                        std::cout << "When: " << str_when << " (code: " << when << "); ";
                        std::cout << std::endl;
                    }

                    global_status.user_scripts[std::pair<unsigned int, unsigned int>(script_state, when)] = str_source;
                }
            }
        }
    }

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Adding the digitizer objects to the Lua runtime environment; ";
        std::cout << "Number of digitizers: " << global_status.digitizers.size() << "; ";
        std::cout << std::endl;
    }

    global_status.lua_manager.update_digitizers(global_status.digitizers);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Finished configuration; ";
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

    global_status.waveforms_buffer_size_Number = 0;

    // Reserve the waveforms_buffer in order to have a good starting size of its buffer
    global_status.waveforms_buffer.reserve(global_status.waveforms_buffer_size_max_Number
                                           * (waveform_header_size() + sizeof(uint16_t) * defaults_absp_waveforms_expected_number_of_samples));

    return true;
}

/******************************************************************************/
/* Specific actions                                                           */
/******************************************************************************/

state actions::start(status &global_status)
{
    global_status.status_msg_ID = 0;
    global_status.data_msg_ID = 0;

    return states::create_context;
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

        return states::communication_error;
    }

    global_status.context = context;

    //std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));
    struct timespec zmq_delay;
    zmq_delay.tv_sec = defaults_abcd_zmq_delay / 1000;
    zmq_delay.tv_nsec = (defaults_abcd_zmq_delay % 1000) * 1000000L;
    nanosleep(&zmq_delay, NULL);

    return states::create_sockets;
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

        return states::communication_error;
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

        return states::communication_error;
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

        return states::communication_error;
    }

    global_status.status_socket = status_socket;
    global_status.data_socket = data_socket;
    global_status.commands_socket = commands_socket;

    //std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));
    struct timespec zmq_delay;
    zmq_delay.tv_sec = defaults_abcd_zmq_delay / 1000;
    zmq_delay.tv_nsec = (defaults_abcd_zmq_delay % 1000) * 1000000L;
    nanosleep(&zmq_delay, NULL);

    return states::bind_sockets;
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

        return states::communication_error;
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

        return states::communication_error;
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

        return states::communication_error;
    }

    //std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));
    struct timespec zmq_delay;
    zmq_delay.tv_sec = defaults_abcd_zmq_delay / 1000;
    zmq_delay.tv_nsec = (defaults_abcd_zmq_delay % 1000) * 1000000L;
    nanosleep(&zmq_delay, NULL);

    return states::create_control_unit;
}

state actions::create_control_unit(status &global_status)
{
    const int validation = ADQAPI_ValidateVersion(ADQAPI_VERSION_MAJOR, ADQAPI_VERSION_MINOR);

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "API validation: " << validation << "; ";
        std::cout << std::endl;
    }

    if (validation == -1) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << " ADQAPI version is incompatible. The application needs to be recompiled and relinked against the installed ADQAPI; ";
        std::cout << std::endl;

        return states::configure_error;
    }
    if (validation == -2) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << WRITE_YELLOW << "WARNING" << WRITE_NC << " ADQAPI version is backwards compatible. It's suggested to recompile and relink the application against the installed ADQAPI; ";
        std::cout << std::endl;
    }

    const uint32_t API_revision = ADQAPI_GetRevision();

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "API revision: " << (unsigned int)API_revision << "; ";
        std::cout << std::endl;
    }

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Creating the ADQ control unit; ";
        std::cout << std::endl;
    }

    global_status.adq_cu_ptr = CreateADQControlUnit();
    if(!global_status.adq_cu_ptr)
    {
        std::cout << "Failed to create adq_cu!" << std::endl;

        return states::configure_error;
    }

    // This creates a file with the error trace when the program is executed
    //if (global_status.verbosity > 0)
    //{
    //    char time_buffer[BUFFER_SIZE];
    //    time_string(time_buffer, BUFFER_SIZE, NULL);
    //    std::cout << '[' << time_buffer << "] ";
    //    std::cout << "Enabling error trace; ";
    //    std::cout << std::endl;
    //}

    //ADQControlUnit_EnableErrorTrace(global_status.adq_cu_ptr, LOG_LEVEL_INFO, ".");
    // This is to enable all possible log levels
    //ADQControlUnit_EnableErrorTrace(global_status.adq_cu_ptr, 0x7FFFFFFF, ".");

    return states::create_digitizer;
}

state actions::create_digitizer(status &global_status)
{
    const bool success = actions::generic::create_digitizer(global_status);

    if (global_status.identification_only) {
        return states::clear_memory;
    }

    if (success)
    {
        return states::read_config;
    }
    else
    {
        return states::configure_error;
    }
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
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": Parse error while reading config file: ";
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

        return states::parse_error;
    }
    else
    {
        global_status.config = new_config;

        return states::configure_digitizer;
    }
}

state actions::recreate_digitizer(status &global_status)
{
    const bool success = actions::generic::create_digitizer(global_status);

    if (success)
    {
        return states::configure_digitizer;
    }
    else
    {
        return states::configure_error;
    }
}

state actions::configure_digitizer(status &global_status)
{
    const bool success = actions::generic::configure_digitizer(global_status);

    if (success)
    {
        return states::allocate_memory;
    }
    else
    {
        return states::configure_error;
    }
}

state actions::reconfigure_destroy_digitizer(status &global_status)
{
    actions::generic::destroy_digitizer(global_status);

    return states::recreate_digitizer;
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

                return states::start_acquisition;
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

                return states::configure_digitizer;
            } else if (command == std::string("specific") && json_arguments) {
                const char *cstr_serial = json_string_value(json_object_get(json_arguments, "serial"));
                const std::string serial = (cstr_serial) ? std::string(cstr_serial) : std::string();

                for (unsigned int digitizer_index = 0; digitizer_index < global_status.digitizers.size(); digitizer_index++)
                {
                    auto digitizer = global_status.digitizers[digitizer_index];

                    if ((digitizer)->GetName() == serial) {
                        if (global_status.verbosity > 0)
                        {
                            char time_buffer[BUFFER_SIZE];
                            time_string(time_buffer, BUFFER_SIZE, NULL);
                            std::cout << '[' << time_buffer << "] ";
                            std::cout << "Found matching card: " << serial << "; ";
                            std::cout << std::endl;
                            std::cout << '[' << time_buffer << "] ";
                            std::cout << "Forwarding command; ";
                            std::cout << std::endl;
                        }

                        const char *cstr_specific_command = json_string_value(json_object_get(json_arguments, "command"));
                        const std::string specific_command = (cstr_serial) ? std::string(cstr_specific_command) : std::string();

                        const std::string event_description = "Specific command to " + serial + ((cstr_serial) ? (": " + specific_command) : std::string());

                        json_t *json_event_message = json_object();

                        json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
                        json_object_set_new_nocheck(json_event_message, "event", json_string(event_description.c_str()));

                        actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

                        json_decref(json_event_message);

                        (digitizer)->SpecificCommand(json_arguments);
                    }
                }
            } else if (command == std::string("off")) {
                return states::clear_memory;
            } else if (command == std::string("quit")) {
                return states::clear_memory;
            }
        }
    }

    json_decref(json_message);

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    if (now - global_status.last_publication > std::chrono::seconds(defaults_abcd_publish_timeout))
    {
        return states::publish_status;
    }

    return states::receive_commands;
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

    // TODO: Check status
    //if () {
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

    return states::receive_commands;
}

state actions::allocate_memory(status &global_status)
{
    const bool success = actions::generic::allocate_memory(global_status);

    if (success)
    {
        return states::publish_status;
    }
    else
    {
        return states::digitizer_error;
    }
}

state actions::restart_allocate_memory(status &global_status)
{
    const bool success = actions::generic::allocate_memory(global_status);

    if (success)
    {
        return states::start_acquisition;
    }
    else
    {
        return states::restart_configure_error;
    }
}

state actions::stop(status&)
{
    return states::stop;
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
        std::cout << "Starting acquisition; ";
        std::cout << std::endl;
    }

    for (auto value = global_status.digitizers_user_ids.begin(); value != global_status.digitizers_user_ids.end(); ++value)
    {
        const unsigned int digitizer_index = value->first;
        //const unsigned int user_id = value->second;

        actions::generic::start_acquisition(global_status, digitizer_index);
        actions::generic::rearm_trigger(global_status, digitizer_index);

        // Sometimes when the digitizers are started, the controller PC shuts
        // itself off abruptly. It has happened both with an internal controller
        // in a PXI chassis as well as with an external desktop connected to the PXI bus.
        // I try to put a bit of delay between the starts to try to solve this,
        // maybe if they are started too close together they interfere with each
        // other.
        std::this_thread::sleep_for(std::chrono::milliseconds(defaults_absp_start_acquisition_delay));
    }

    global_status.start_time = start_time;

    return states::acquisition_receive_commands;
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

                return states::stop_publish_events;
            } else if (command == std::string("simulate_error")) {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "########################################################### SIMULATED ERROR! ###";
                std::cout << std::endl;

                json_t *json_event_message = json_object();

                json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
                json_object_set_new_nocheck(json_event_message, "event", json_string("Simulated error"));

                actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

                json_decref(json_event_message);


                return states::acquisition_error;
            }
        }

        json_decref(json_message);

    }

    return states::read_data;
}

state actions::read_data(status &global_status)
{
    const unsigned int verbosity = global_status.verbosity;

    bool is_error = false;

    // Gather data from the available digitizers
    for (auto value = global_status.digitizers_user_ids.begin(); value != global_status.digitizers_user_ids.end(); ++value)
    {
        const unsigned int digitizer_index = value->first;
        const unsigned int user_id = value->second;
        auto digitizer = global_status.digitizers[digitizer_index];

        const bool is_ready = digitizer->AcquisitionReady();
        const bool is_overflow = digitizer->DataOverflow();

        if (verbosity > 0) {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Polling board: " << digitizer->GetName() << " (user_id: " << user_id << ", digitizer_index: " << digitizer_index << "); ";
            std::cout << "Acquisition ready: " << (is_ready ? "yes" : "no") << "; ";
            std::cout << "Overflow: " << (is_overflow ? "yes" : "no") << "; ";
            std::cout << std::endl;
        }

        if (is_overflow) {
            digitizer->ResetOverflow();

            std::string error_string = "Data overflow in digitizer: ";
            error_string += digitizer->GetName();

            json_t *json_event_message = json_object();

            json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
            json_object_set_new_nocheck(json_event_message, "error", json_string(error_string.c_str()));

            actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

            json_decref(json_event_message);

            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": " << error_string;
            std::cout << std::endl;

        }

        if (is_ready) {
            if (verbosity > 0) {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Getting waveforms from card; ";
                std::cout << std::endl;
            }

            std::vector<struct event_waveform> waveforms;

            const auto get_data_start = std::chrono::high_resolution_clock::now();

            const int retval = digitizer->GetWaveformsFromCard(waveforms);

            const auto get_data_end = std::chrono::high_resolution_clock::now();
            auto delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(get_data_end - get_data_start);

            unsigned int waveforms_size = waveforms.size();

            if (verbosity > 0) {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Waveforms download: " << (retval == DIGITIZER_SUCCESS ? "success" : "failure") << "; ";
                std::cout << "waveforms number: " << waveforms_size << "; ";
                std::cout << "Time required: " << delta_time.count() << " ms; ";
                std::cout << std::endl;
            }

            if (retval == DIGITIZER_FAILURE) {
                std::string error_string = "Data fetch failure in digitizer: ";
                error_string += digitizer->GetName();

                json_t *json_event_message = json_object();

                json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
                json_object_set_new_nocheck(json_event_message, "error", json_string(error_string.c_str()));

                actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

                json_decref(json_event_message);

                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << WRITE_RED << "ERROR" << WRITE_NC << ": " << error_string;
                std::cout << std::endl;

                is_error = true;
            } else {
                for (unsigned int waveform_index = 0; waveform_index < waveforms_size; waveform_index++) {
                    struct event_waveform this_waveform = waveforms[waveform_index];

                    const uint8_t global_channel = this_waveform.channel + user_id * (digitizer)->GetChannelsNumber();

                    this_waveform.channel = global_channel;
                    global_status.counts[global_channel] += 1;
                    global_status.partial_counts[global_channel] += 1;
                    global_status.waveforms_buffer_size_Number += 1;


                    const size_t current_waveform_buffer_size = global_status.waveforms_buffer.size();
                    const size_t this_waveform_size = waveform_size(&this_waveform);

                    if (verbosity > 2) {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << WRITE_RED << "Storing waveform in buffer; " << WRITE_NC;
                        std::cout << "Waveform index: " << waveform_index << "; ";
                        std::cout << "channel: " << (unsigned int)this_waveform.channel << "; ";
                        std::cout << "samples: " << (unsigned int)this_waveform.samples_number << "; ";
                        std::cout << "buffer pointer: " << static_cast<void*>(this_waveform.buffer) << "; ";
                        std::cout << "Waveforms buffer size: " << current_waveform_buffer_size << "; ";
                        std::cout << "Waveform size: " << this_waveform_size << "; ";
                        std::cout << std::endl;
                    }

                    global_status.waveforms_buffer.resize(current_waveform_buffer_size + this_waveform_size);

                    memcpy(global_status.waveforms_buffer.data() + current_waveform_buffer_size,
                           reinterpret_cast<void*>(waveform_serialize(&this_waveform)),
                           this_waveform_size);

                    waveform_destroy_samples(&this_waveform);
                }

                actions::generic::rearm_trigger(global_status, digitizer_index);
            }
        }
    }

    const size_t waveforms_buffer_size_Bytes = global_status.waveforms_buffer.size();

    if (verbosity > 0) {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Waveforms buffer size: " << global_status.waveforms_buffer_size_Number << " (" << waveforms_buffer_size_Bytes << " B); ";
        std::cout << std::endl;
    }

    if (is_error) {
        return states::acquisition_error;
    }

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();

    if (now - global_status.last_publication > std::chrono::seconds(defaults_abcd_publish_timeout) ||
        global_status.waveforms_buffer_size_Number >= global_status.waveforms_buffer_size_max_Number) {
        return states::publish_events;
    }

    return states::read_data;
}

state actions::publish_events(status &global_status)
{
    actions::generic::publish_events(global_status);

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    if (now - global_status.last_publication > std::chrono::seconds(defaults_abcd_publish_timeout))
    {
        return states::acquisition_publish_status;
    }

    return states::read_data;
}

state actions::stop_publish_events(status &global_status)
{
    actions::generic::publish_events(global_status);

    return states::stop_acquisition;
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

    // TODO: Check status
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

    return states::acquisition_receive_commands;
}

state actions::stop_acquisition(status &global_status)
{
    actions::generic::stop_acquisition(global_status);

    auto delta_time = std::chrono::duration_cast<std::chrono::duration<long int>>(global_status.stop_time - global_status.start_time);
    const std::string event_message = "Stop acquisition (duration: " + std::to_string((long long unsigned int)delta_time.count()) + " s)";

    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string(event_message.c_str()));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::receive_commands;
}

state actions::clear_memory(status &global_status)
{
    actions::generic::clear_memory(global_status);

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Clearing the json configuration; ";
        std::cout << std::endl;
    }

    // Remember to clean up the json configuration
    json_decref(global_status.config);
    global_status.config = NULL;

    return states::destroy_digitizer;
}

state actions::reconfigure_clear_memory(status &global_status)
{
    actions::generic::clear_memory(global_status);

    return states::reconfigure_destroy_digitizer;
}

state actions::destroy_digitizer(status &global_status)
{
    actions::generic::destroy_digitizer(global_status);

    return states::destroy_control_unit;
}

state actions::destroy_control_unit(status &global_status)
{
    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Deleting the ADQ control unit; ";
        std::cout << std::endl;
    }

    DeleteADQControlUnit(global_status.adq_cu_ptr);
    global_status.adq_cu_ptr = NULL;

    return states::close_sockets;
}

state actions::restart_publish_events(status &global_status)
{
    actions::generic::publish_events(global_status);

    return states::restart_stop_acquisition;
}

state actions::restart_stop_acquisition(status &global_status)
{
    actions::generic::stop_acquisition(global_status);

    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string("Stop acquisition for a restart"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::restart_clear_memory;
}

state actions::restart_clear_memory(status &global_status)
{
    actions::generic::clear_memory(global_status);

    return states::restart_destroy_digitizer;
}

state actions::restart_destroy_digitizer(status &global_status)
{
    actions::generic::destroy_digitizer(global_status);

    return states::restart_create_digitizer;
}

state actions::restart_create_digitizer(status &global_status)
{
    const bool success = actions::generic::create_digitizer(global_status);

    if (success)
    {
        return states::restart_configure_digitizer;
    }
    else
    {
        return states::restart_configure_error;
    }
}

state actions::restart_configure_digitizer(status &global_status)
{
    const bool success = actions::generic::configure_digitizer(global_status);

    if (success)
    {
        return states::restart_allocate_memory;
    }
    else
    {
        return states::restart_configure_error;
    }
}

state actions::close_sockets(status &global_status)
{
    //std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));
    struct timespec zmq_delay;
    zmq_delay.tv_sec = defaults_abcd_zmq_delay / 1000;
    zmq_delay.tv_nsec = (defaults_abcd_zmq_delay % 1000) * 1000000L;
    nanosleep(&zmq_delay, NULL);

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

    return states::destroy_context;
}

state actions::destroy_context(status &global_status)
{
    //std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));
    struct timespec zmq_delay;
    zmq_delay.tv_sec = defaults_abcd_zmq_delay / 1000;
    zmq_delay.tv_nsec = (defaults_abcd_zmq_delay % 1000) * 1000000L;
    nanosleep(&zmq_delay, NULL);

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

    return states::stop;
}

state actions::communication_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Communication error"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::close_sockets;
}

state actions::parse_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Config parse error"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::close_sockets;
}

state actions::configure_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Configure error"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::reconfigure_destroy_digitizer;
}

state actions::digitizer_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Digitizer error"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::reconfigure_clear_memory;
}

state actions::acquisition_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Acquisition error"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::restart_publish_events;
}

state actions::restart_configure_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Restart configure error"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::restart_destroy_digitizer;
}
