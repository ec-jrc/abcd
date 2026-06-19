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

#include <chrono>
#include <vector>
#include <map>
// For std::pair
#include <utility>
#include <cmath>
#include <cstring>
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

#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

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
#include "ADQ36_FWDAQ.hpp"

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

            absp_logger_console->info("Sending waveforms buffer; Topic: {}; waveforms: {};", topic, waveforms_buffer_size_Bytes);

        const bool result = send_byte_message(global_status.data_output_socket,
                                              topic.c_str(),
                                              global_status.waveforms_buffer.data(),
                                              waveforms_buffer_size_Bytes,
                                              0);

        if (result == EXIT_FAILURE)
        {
            absp_logger_error->error("ZeroMQ Error publishing events");
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

    char time_buffer[std::size("  yyyy-mm-ddThh:mm:ssZ+00:00  ")];
    time_string(time_buffer, std::size(time_buffer), NULL);

    json_object_set_new(status_message, "module", json_string("abcd"));
    json_object_set_new(status_message, "timestamp", json_string(time_buffer));
    json_object_set_new(status_message, "msg_ID", json_integer(status_msg_ID));

    char *output_buffer = json_dumps(status_message, JSON_COMPACT);

    if (!output_buffer)
    {
        absp_logger_error->error("Unable to allocate status output buffer;");
    }
    else
    {
        size_t total_size = strlen(output_buffer);

        topic += "_v0_s";
        topic += std::to_string((long long unsigned int)total_size);

            absp_logger_console->info("Sending status message; Topic: {}; buffer size: {}; message: {};", topic, total_size, output_buffer);

        send_byte_message(status_socket, topic.c_str(), output_buffer, total_size, 0);

        free(output_buffer);
    }

    global_status.status_msg_ID += 1;
}

bool actions::generic::create_control_unit(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string("Control unit creation"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    const int validation = ADQAPI_ValidateVersion(ADQAPI_VERSION_MAJOR, ADQAPI_VERSION_MINOR);

        absp_logger_console->info("API validation: {};", validation);

    if (validation == -1) {
        absp_logger_error->error("ERROR ADQAPI version is incompatible. The application needs to be recompiled and relinked against the installed ADQAPI;");

        return false;
    } else if (validation == -2) {
        absp_logger_console->warn("WARNING ADQAPI version is backwards compatible. It's suggested to recompile and relink the application against the installed ADQAPI;");
    }

    const char *API_revision = ADQAPI_GetRevisionString();

    absp_logger_console->info("API revision: {};", API_revision);

    absp_logger_console->info("Creating the ADQ control unit;");

    global_status.adq_cu_ptr = CreateADQControlUnit();
    if(!global_status.adq_cu_ptr)
    {
        absp_logger_error->error("Failed to create adq_cu!");

        return false;
    }

    if (absp_logger_console->should_log(spdlog::level::debug))
    {
        absp_logger_console->debug("Enabling error trace at level INFO;");

        // This creates a file when the program is executed
        ADQControlUnit_EnableErrorTrace(global_status.adq_cu_ptr, LOG_LEVEL_INFO, ".");
    }

    if (absp_logger_console->should_log(spdlog::level::trace))
    {
        absp_logger_console->trace("Enabling full error trace of the ADQAPI;");

        // This is to enable all possible log levels
        ADQControlUnit_EnableErrorTrace(global_status.adq_cu_ptr, 0x7FFFFFFF, ".");
    }

    return true;
}

bool actions::generic::destroy_control_unit(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string("Control unit deactivation"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

        absp_logger_console->info("Deleting the ADQ control unit;");

    DeleteADQControlUnit(global_status.adq_cu_ptr);
    global_status.adq_cu_ptr = NULL;

    return true;
}

int actions::generic::start_acquisition(status &global_status, unsigned int digitizer_index)
{
        absp_logger_console->info("#### Starting acquisition!!! ");

    const unsigned int user_id = global_status.digitizers_user_ids.at(digitizer_index);
    auto digitizer = global_status.digitizers[digitizer_index];

        absp_logger_console->info("Starting acquisition of card id: {}; name: {};", user_id, digitizer->GetName());

    return digitizer->StartAcquisition();
}

void actions::generic::rearm_trigger(status &global_status, unsigned int digitizer_index)
{
        absp_logger_console->info("#### Rearming trigger!!! ");

    const unsigned int user_id = global_status.digitizers_user_ids.at(digitizer_index);
    auto digitizer = global_status.digitizers[digitizer_index];

        absp_logger_console->info("Arming trigger of card: id: {}; name: {};", user_id, digitizer->GetName());

    digitizer->RearmTrigger();
}

void actions::generic::stop_acquisition(status &global_status)
{
        absp_logger_console->info("#### Stopping acquisition!!! ");

    for (auto value = global_status.digitizers_user_ids.begin(); value != global_status.digitizers_user_ids.end(); ++value)
    {
        const unsigned int digitizer_index = value->first;
        const unsigned int user_id = value->second;
        auto digitizer = global_status.digitizers[digitizer_index];

            absp_logger_console->info("Arming trigger of card: id: {}; name: {};", user_id, digitizer->GetName());

        digitizer->StopAcquisition();
    }

    std::chrono::time_point<std::chrono::system_clock> stop_time = std::chrono::system_clock::now();
    auto delta_time = std::chrono::duration_cast<std::chrono::duration<long int>>(stop_time - global_status.start_time);

    global_status.stop_time = stop_time;

        absp_logger_console->info("Run time: {}", delta_time.count());
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
        absp_logger_console->info("Destroying digitizers;");

    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string("Digitizers deactivation"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    for (unsigned int index = 0; index < global_status.digitizers.size(); index++)
    {
        ABCD::Digitizer *digitizer = global_status.digitizers[index];

            absp_logger_console->info("Destroying card: {};", digitizer->GetName());

        delete digitizer;
    }

    global_status.digitizers.clear();


    // TODO: Reset USB3 devices with ADQControlUnit_ResetDevice() and reset level 18
}

bool actions::generic::create_digitizer(status &global_status)
{
    const std::chrono::time_point<std::chrono::system_clock> initialization_global_start = std::chrono::system_clock::now();

        absp_logger_console->info("Initializing digitizers;");

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
            absp_logger_error->error("ADQAPI error: {},   description: ListDevices failed! The linked ADQAPI is not for the correct OS, please select correct x86/x64 platform when building.;", error_code);
        }
    }

    unsigned int number_of_ADQ214 = 0;
    unsigned int number_of_ADQ412 = 0;
    unsigned int number_of_ADQ14 = 0;
    unsigned int number_of_ADQ36 = 0;
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
            case PID_ADQ36:
                number_of_ADQ36 += 1;
                break;
            default:
                number_of_other += 1;
        }
    }

    const unsigned int number_of_ADQs = number_of_ADQ214 + number_of_ADQ412 + number_of_ADQ14;

        absp_logger_console->info("Number of devices: {}; Number of ADQs: {}; Number of ADQ214: {}; Number of ADQ412: {}; Number of ADQ14: {}; Number of ADQ36: {}; Number of other devices: {};", number_of_devices, number_of_ADQs, number_of_ADQ214, number_of_ADQ412, number_of_ADQ14, number_of_ADQ36, number_of_other);

    std::string initialization_begin_string = "Digitizers initialization: ";
    initialization_begin_string += "Number of ADQs: " + std::to_string(number_of_ADQs) + "; ";

    if (number_of_ADQ412 > 0) {
        initialization_begin_string += "Number of ADQ412: " + std::to_string(number_of_ADQ412) + "; ";
    }
    if (number_of_ADQ214 > 0) {
        initialization_begin_string += "Number of ADQ214: " + std::to_string(number_of_ADQ214) + "; ";
    }
    if (number_of_ADQ14 > 0) {
        initialization_begin_string += "Number of ADQ14: " + std::to_string(number_of_ADQ14) + "; ";
    }
    if (number_of_ADQ36 > 0) {
        initialization_begin_string += "Number of ADQ36: " + std::to_string(number_of_ADQ36) + "; ";
    }

    json_t *json_event_begin_message = json_object();

    json_object_set_new_nocheck(json_event_begin_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_begin_message, "event", json_string(initialization_begin_string.c_str()));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_begin_message);

    json_decref(json_event_begin_message);

    for (unsigned int device_index = 0; device_index < number_of_devices; device_index++) {
            absp_logger_console->info("Device index: {};", device_index);
            absp_logger_console->info("Product id: {};", ADQlist[device_index].ProductID);
            absp_logger_console->info("Address: {} {};", ADQlist[device_index].AddressField1, ADQlist[device_index].AddressField2);
            absp_logger_console->info("Device file: {};", ADQlist[device_index].DevFile);
            absp_logger_console->info("Opened interface: {};", ADQlist[device_index].DeviceInterfaceOpened);
            absp_logger_console->info("Set-up completed: {};", ADQlist[device_index].DeviceSetupCompleted);

            absp_logger_console->info("Opening device interface establishing a communication channel for device: {};", device_index);

        void *adq_cu_ptr = global_status.adq_cu_ptr;

        CHECKZERO(ADQControlUnit_OpenDeviceInterface(adq_cu_ptr, device_index));

            absp_logger_console->info("Setting-up device: {};", device_index);

        CHECKZERO(ADQControlUnit_SetupDevice(adq_cu_ptr, device_index));

            absp_logger_console->info("Information about device: {};", device_index);
            absp_logger_console->info("ADQ type: {}; Board product name: {};", ADQ_GetADQType(global_status.adq_cu_ptr, device_index + 1), ADQ_GetBoardProductName(global_status.adq_cu_ptr, device_index + 1));
            // This is not supported for all cards, and it might generate a
            // segfault since it returns a string that might not have been
            // initialized.
            //logger_console->info("Card option: {};", ADQ_GetCardOption(global_status.adq_cu_ptr, device_index + 1));

            unsigned int family = 0;
            CHECKZERO(ADQ_GetProductFamily(global_status.adq_cu_ptr, device_index + 1, &family));
            absp_logger_console->info("Product ID: {}; Product family: {};", ADQ_GetProductID(global_status.adq_cu_ptr, device_index + 1), family);

            uint32_t *revision = ADQ_GetRevision(global_status.adq_cu_ptr, device_index + 1);
            std::string string_revision = "";

            for (int i = 0; i < 6; i++) {
                string_revision += std::to_string((unsigned int)revision[i]) + ", ";
            }
            absp_logger_console->info("Revision: {}", string_revision);
            try {
                absp_logger_console->info("Firmware type: {}", ADQ_descriptions::ADQ_firmware_revisions.at(revision[0]));
            } catch (const std::out_of_range& error) {
                absp_logger_console->info("Firmware type: unknown");
            }

            absp_logger_console->info("Serial number: {};", ADQ_GetBoardSerialNumber(global_status.adq_cu_ptr, device_index + 1));

        if (ADQlist[device_index].ProductID == PID_ADQ214) {
            // WARNING: boards numbering start from 1 in the next functions
            const int adq214_index = device_index + 1;

            ABCD::ADQ214 *adq214_ptr = new ABCD::ADQ214(global_status.adq_cu_ptr, adq214_index);

            adq214_ptr->Initialize();

            global_status.digitizers.push_back(adq214_ptr);
        } else if (ADQlist[device_index].ProductID == PID_ADQ412) {
            // WARNING: boards numbering start from 1 in the next functions
            const int adq412_index = device_index + 1;

            ABCD::ADQ412 *adq412_ptr = new ABCD::ADQ412(global_status.adq_cu_ptr, adq412_index);

            adq412_ptr->Initialize();

            global_status.digitizers.push_back(adq412_ptr);
        } else if (ADQlist[device_index].ProductID == PID_ADQ14) {
            // WARNING: boards numbering start from 1 in the next functions
            const int adq14_index = device_index + 1;

            // Check that the Pulse Detect firmware is enabled on this device.
            const int has_FWPD = ADQ_HasFeature(global_status.adq_cu_ptr, adq14_index, "FWPD");

            // These are just for information
            if (has_FWPD == 1) {
                    absp_logger_console->info("ADQ14 (index {}) Has the Pulse detect firmware;", adq14_index);
            } else if (has_FWPD == -1) {
                absp_logger_console->warn("ADQ14 (index {}) does not have the Pulse Detect firmware;", adq14_index);
            } else if (has_FWPD == 0) {
                absp_logger_console->warn("ADQ14 (index {}) did not respond to the firmware request;", adq14_index);
            } else {
                absp_logger_error->error("ADQ14 (index {}) unexpected value from the firmware request: {};", adq14_index, has_FWPD);
            }

            // FIXME: This might be more reliable with older cards, but we would
            //        need to know all the firmware versions
            //int *revision = ADQ_GetRevision(global_status.adq_cu_ptr, device_index + 1);
            //if (ADQ_descriptions::ADQ_firmware_revisions.at(revision[0]) == "ADQ14_FWPD")

            if (has_FWPD) {
                ABCD::ADQ14_FWPD *adq14_ptr = new ABCD::ADQ14_FWPD(global_status.adq_cu_ptr, adq14_index);

                adq14_ptr->Initialize();

                global_status.digitizers.push_back(adq14_ptr);


            } else {
                ABCD::ADQ14_FWDAQ *adq14_ptr = new ABCD::ADQ14_FWDAQ(global_status.adq_cu_ptr, adq14_index);

                adq14_ptr->Initialize();

                global_status.digitizers.push_back(adq14_ptr);
            }
        } else if (ADQlist[device_index].ProductID == PID_ADQ36) {
            // WARNING: boards numbering start from 1 in the next functions
            const int adq36_index = device_index + 1;

            ABCD::ADQ36_FWDAQ *adq36_ptr = new ABCD::ADQ36_FWDAQ(global_status.adq_cu_ptr, adq36_index);

            adq36_ptr->Initialize();

            global_status.digitizers.push_back(adq36_ptr);
        }
    }

    const int number_of_failed_devices = ADQControlUnit_GetFailedDeviceCount(global_status.adq_cu_ptr);

    if (number_of_failed_devices > 0) {
        char error_cstring[512];
        unsigned int error_code = ADQControlUnit_GetLastFailedDeviceErrorWithText(global_status.adq_cu_ptr, error_cstring);

        ADQControlUnit_ClearLastFailedDeviceError(global_status.adq_cu_ptr);
        absp_logger_error->error("ADQAPI error: {},   description: {};", error_code, error_cstring);

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

    const std::chrono::time_point<std::chrono::system_clock> initialization_global_stop = std::chrono::system_clock::now();
    const auto initialization_global_delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(initialization_global_stop - initialization_global_start);

    std::string initialization_done_string = "Digitizers initialization completed";
    initialization_done_string += " (time: ";
    initialization_done_string += std::to_string(initialization_global_delta_time.count());
    initialization_done_string += " ms)";

    json_t *json_event_done_message = json_object();

    json_object_set_new_nocheck(json_event_done_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_done_message, "event", json_string(initialization_done_string.c_str()));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_done_message);

    json_decref(json_event_done_message);


    return true;
}

bool actions::generic::configure_digitizer(status &global_status)
{
        absp_logger_console->info("Configuring digitizer;");

    global_status.digitizers_user_ids.clear();
    global_status.user_scripts.clear();

    // -------------------------------------------------------------------------
    //  Starting the global configuration
    // -------------------------------------------------------------------------
    const std::chrono::time_point<std::chrono::system_clock> configuration_global_start = std::chrono::system_clock::now();

    json_t *config = global_status.config;

    json_t *json_global = json_object_get(config, "global");

    if (!json_global)
    {
        json_object_set_new_nocheck(config, "global", json_object());
        json_global = json_object_get(config, "global");

            absp_logger_error->warn("Missing \"global\" entry in configuration;");


    }

    if (json_global != NULL && json_is_object(json_global))
    {
        // WARNING: The key has a different spelling for the user convenience
        int waveforms_buffer_size_max_Number = json_number_value(json_object_get(json_global, "waveforms_buffer_max_size"));

        if (waveforms_buffer_size_max_Number <= 0) {
            waveforms_buffer_size_max_Number = defaults_absp_waveforms_buffer_size_max_Number;
        }

            absp_logger_console->info("Waveforms buffer max size: {};", waveforms_buffer_size_max_Number);

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

                absp_logger_console->info("Found card: {}; user id: {};", serial, user_id);

            const bool enabled = json_is_true(json_object_get(card, "enable"));

            absp_logger_console->info("Card is {}", enabled ? "enabled" : "disabled");

            for (unsigned int digitizer_index = 0; digitizer_index < global_status.digitizers.size() && enabled; digitizer_index++)
            {
                auto digitizer = global_status.digitizers[digitizer_index];

                if ((digitizer)->GetName() == serial) {
                        absp_logger_console->info("Found matching card: {};", serial);
                        absp_logger_console->info("Parsing configuration;");

                    (digitizer)->ReadConfig(card);

                        absp_logger_console->info("Applying configuration;");

                    const std::chrono::time_point<std::chrono::system_clock> configuration_single_start = std::chrono::system_clock::now();

                    const int config_result = (digitizer)->Configure();

                        absp_logger_console->info("Finished configuration;");
                        absp_logger_console->info("digitizer_index: {}; user id: {}; config result: {};", digitizer_index, user_id, (config_result == DIGITIZER_SUCCESS ? "success" : "failure"));

                    const unsigned int this_max_channel_number = (user_id + 1) * (digitizer)->GetChannelsNumber();

                    if (max_channel_number < this_max_channel_number) {
                        max_channel_number = this_max_channel_number;
                    }

                        absp_logger_console->info("Channels number: {}; Max channel number: {};", digitizer->GetChannelsNumber(), this_max_channel_number);

                    global_status.digitizers_user_ids[digitizer_index] = user_id;

                    const std::chrono::time_point<std::chrono::system_clock> configuration_single_stop = std::chrono::system_clock::now();
                    const auto configuration_single_delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(configuration_single_stop - configuration_single_start);

                    if (config_result == DIGITIZER_SUCCESS) {
                        std::string configure_string = "Digitizer configuration, model: ";
                        configure_string += digitizer->GetModel();
                        configure_string += ", serial: ";
                        configure_string += digitizer->GetName();
                        configure_string += ", user id: ";
                        configure_string += std::to_string(user_id);
                        configure_string += " (time: ";
                        configure_string += std::to_string(configuration_single_delta_time.count());
                        configure_string += " ms)";

                        json_t *json_event_message = json_object();

                        json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
                        json_object_set_new_nocheck(json_event_message, "event", json_string(configure_string.c_str()));

                        actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

                        json_decref(json_event_message);
		    } else {
                    	global_status.digitizers_user_ids[digitizer_index] = user_id;

                    	const std::chrono::time_point<std::chrono::system_clock> configuration_single_stop = std::chrono::system_clock::now();
                    	const auto configuration_single_delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(configuration_single_stop - configuration_single_start);

                    	std::string configure_string = "Digitizer configuration, model: ";
                    	configure_string += digitizer->GetModel();
                    	configure_string += ", serial: ";
                    	configure_string += digitizer->GetName();
                    	configure_string += ", user id: ";
                    	configure_string += std::to_string(user_id);
                    	configure_string += ", error: ";
                    	configure_string += ADQ_descriptions::ADQ36_errors.at(config_result);
                    	configure_string += " (time: ";
                    	configure_string += std::to_string(configuration_single_delta_time.count());
                    	configure_string += " ms)";

                    	json_t *json_event_message = json_object();

                    	json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
                    	json_object_set_new_nocheck(json_event_message, "error", json_string(configure_string.c_str()));

                    	actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

                    	json_decref(json_event_message);
		    }

                    break;
                }
            }
        }
    }

    global_status.channels_number = max_channel_number;

        absp_logger_console->info("Channels number: {};", max_channel_number);

        absp_logger_console->info("Digitizers configuration completed successfully!");

    const std::chrono::time_point<std::chrono::system_clock> configuration_global_stop = std::chrono::system_clock::now();
    const auto configuration_global_delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(configuration_global_stop - configuration_global_start);

    std::string configure_string = "Digitizers configuration completed";
    configure_string += " (time: ";
    configure_string += std::to_string(configuration_global_delta_time.count());
    configure_string += " ms)";

    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string(configure_string.c_str()));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    json_t *json_scripts = json_object_get(config, "scripts");

    if (json_scripts != NULL && json_is_array(json_scripts))
    {
        size_t index;
        json_t *json_script;

        json_array_foreach(json_scripts, index, json_script) {
            const bool enabled = json_is_true(json_object_get(json_script, "enable"));

            absp_logger_console->info("Script is {}", enabled ? "enabled" : "disabled");

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
                        absp_logger_console->info("The source of this script is in the file: {};", str_source);

                    std::stringstream buffer;
                    buffer << source_file.rdbuf();

                    str_source = buffer.str();
                } else {
                        absp_logger_console->info("The source of this script is: {};", str_source);
                }

                for (const auto& script_state : script_states) {
                        absp_logger_console->info("Found script for state: {}; When: {} (code: {});", script_state, str_when, when);

                    global_status.user_scripts[std::pair<unsigned int, unsigned int>(script_state, when)] = str_source;
                }
            }
        }
    }

        absp_logger_console->info("Adding the digitizer objects to the Lua runtime environment; Number of digitizers: {};", global_status.digitizers.size());

    global_status.lua_manager.update_digitizers(global_status.digitizers);

        absp_logger_console->info("Finished configuration;");

    return true;
}

bool actions::generic::allocate_memory(status &global_status)
{
    global_status.counts.clear();
    global_status.counts.resize(global_status.channels_number, 0);
    global_status.partial_counts.clear();
    global_status.partial_counts.resize(global_status.channels_number, 0);
    global_status.ICR_prev_counts.clear();
    global_status.ICR_prev_counts.resize(global_status.channels_number, 0);
    global_status.ICR_curr_counts.clear();
    global_status.ICR_curr_counts.resize(global_status.channels_number, 0);

        absp_logger_console->info("Memory allocation");

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
    global_status.counter_restarts = 0;
    global_status.counter_resets = 0;

    return states::create_context;
}

state actions::create_context(status &global_status)
{
    // Creates a ØMQ context
    void *context = zmq_ctx_new();
    if (!context)
    {
        // No errors are defined for this function
        absp_logger_error->error("ZeroMQ Error on context creation");

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
        absp_logger_error->error("ZeroMQ Error on status socket creation: {}", zmq_strerror(errno));

        return states::communication_error;
    }

    // Creates the data socket
    void *data_socket = zmq_socket(context, ZMQ_PUB);
    if (!data_socket)
    {
        absp_logger_error->error("ZeroMQ Error on data socket creation: {}", zmq_strerror(errno));

        return states::communication_error;
    }

    // Creates the commands socket
    void *commands_socket = zmq_socket(context, ZMQ_PULL);
    if (!commands_socket)
    {
        absp_logger_error->error("ZeroMQ Error on commands socket creation: {}", zmq_strerror(errno));

        return states::communication_error;
    }

    global_status.status_socket = status_socket;
    global_status.data_output_socket = data_socket;
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
    std::string data_address = global_status.data_output_address;
    std::string commands_address = global_status.commands_address;

    // Binds the status socket to its address
    const int s = zmq_bind(global_status.status_socket, status_address.c_str());
    if (s != 0)
    {
        absp_logger_error->error("ZeroMQ Error on status socket binding: {}", zmq_strerror(errno));

        return states::communication_error;
    }

    // Binds the data socket to its address
    const int d = zmq_bind(global_status.data_output_socket, data_address.c_str());
    if (d != 0)
    {
        absp_logger_error->error("ZeroMQ Error on data socket binding: {}", zmq_strerror(errno));

        return states::communication_error;
    }

    // Binds the socket to its address
    const int c = zmq_bind(global_status.commands_socket, commands_address.c_str());
    if (c != 0)
    {
        absp_logger_error->error("ZeroMQ Error on commands socket binding: {}", zmq_strerror(errno));

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
    const bool success = actions::generic::create_control_unit(global_status);

    if (success)
    {
        return states::create_digitizer;
    }
    else
    {
        return states::close_sockets;
    }
}

state actions::create_digitizer(status &global_status)
{
    const bool success = actions::generic::create_digitizer(global_status);

    if (global_status.identification_only) {
        return states::destroy_digitizer;
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
    const std::string config_file_name = global_status.config_filename;

        absp_logger_console->info("Reading config file: {} ", config_file_name);

    json_error_t error;

    json_t *new_config = json_load_file(config_file_name.c_str(), 0, &error);

    if (!new_config)
    {
        absp_logger_error->error("Parse error while reading config file: {} (source: {}, line: {}, column: {}, position: {});", error.text, error.source, error.line, error.column, error.position);

        return states::parse_error;
    }
    else
    {
        global_status.config = new_config;

        return states::configure_digitizer;
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

state actions::reconfigure_clear_memory(status &global_status)
{
    actions::generic::clear_memory(global_status);

    return states::reconfigure_destroy_digitizer;
}

state actions::reconfigure_destroy_digitizer(status &global_status)
{
    actions::generic::destroy_digitizer(global_status);

    return states::reconfigure_create_digitizer;
}

state actions::reconfigure_create_digitizer(status &global_status)
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

state actions::publish_status(status &global_status)
{
        absp_logger_console->info("Publishing status;");

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
    json_object_set_new_nocheck(status_message, "config_file", json_string(global_status.config_filename.c_str()));

    actions::generic::publish_message(global_status, defaults_abcd_status_topic, status_message);

    json_decref(status_message);

    return states::receive_commands;
}

state actions::receive_commands(status &global_status)
{
    void *commands_socket = global_status.commands_socket;

        absp_logger_console->debug("Receiving command... ");

    json_t *json_message = NULL;

    const int result = receive_json_message(commands_socket, NULL, &json_message, false, 0);

    if (!json_message || result == EXIT_FAILURE)
    {
        absp_logger_error->error("Error on receiving JSON commands message");
    }
    else
    {
        const size_t command_ID = json_integer_value(json_object_get(json_message, "msg_ID"));

            absp_logger_console->debug("Received command; Command ID: {};", command_ID);

        json_t *json_command = json_object_get(json_message, "command");
        json_t *json_arguments = json_object_get(json_message, "arguments");

        if (json_command != NULL)
        {
            const std::string command = json_string_value(json_command);

                absp_logger_console->info("Received command: {};", command);

            if (command == std::string("start")) {
                absp_logger_console->info("################################################################### Start!!! ###");

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
                            absp_logger_console->info("Found matching card: {};", serial);
                            absp_logger_console->info("Forwarding command;");

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
            } else if (command == std::string("store_configuration")) {
                const std::string config_file_name = global_status.config_filename;

                const int r = json_dump_file(global_status.config, config_file_name.c_str(), JSON_INDENT(4));

                if (r < 0)
                {
                    std::string event_description = "Unable to store configuration file to: " + config_file_name;
                    absp_logger_error->error("{};", event_description);

                    json_t *json_event_message = json_object();

                    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
                    json_object_set_new_nocheck(json_event_message, "error", json_string(event_description.c_str()));

                    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

                    json_decref(json_event_message);

                } else {
                    const std::string event_description = "Stored configuration to " + config_file_name;

                    json_t *json_event_message = json_object();

                    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
                    json_object_set_new_nocheck(json_event_message, "event", json_string(event_description.c_str()));

                    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

                    json_decref(json_event_message);
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

state actions::start_acquisition(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string("Start acquisition"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    std::chrono::time_point<std::chrono::system_clock> start_time = std::chrono::system_clock::now();

        absp_logger_console->info("Clearing counters;");

    const int channels_number = global_status.channels_number;

    global_status.counts.clear();
    global_status.counts.resize(channels_number, 0);
    global_status.partial_counts.clear();
    global_status.partial_counts.resize(channels_number, 0);
    global_status.ICR_prev_counts.clear();
    global_status.ICR_prev_counts.resize(global_status.channels_number, 0);
    global_status.ICR_curr_counts.clear();
    global_status.ICR_curr_counts.resize(global_status.channels_number, 0);

    // Start acquisition
        absp_logger_console->info("Starting acquisition;");

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

        absp_logger_console->debug("Receiving command... ");

    json_t *json_message = NULL;

    const int result = receive_json_message(commands_socket, NULL, &json_message, false, 0);

    if (!json_message || result == EXIT_FAILURE)
    {
        absp_logger_error->error("Error on receiving JSON commands message");
    }
    else
    {
        const size_t command_ID = json_integer_value(json_object_get(json_message, "msg_ID"));

            absp_logger_console->debug("Received command; Command ID: {};", command_ID);

        json_t *json_command = json_object_get(json_message, "command");

        if (json_command != NULL)
        {
            const std::string command = json_string_value(json_command);

                absp_logger_console->info("Received command: {};", command);

            if (command == std::string("stop")) {
                absp_logger_console->info("#################################################################### Stop!!! ###");

                return states::stop_publish_events;

            } else if (command == std::string("simulate_error")) {
                absp_logger_error->error("########################################################### SIMULATED ERROR! ###");

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

state actions::stop_publish_events(status &global_status)
{
    actions::generic::publish_events(global_status);

    return states::stop_acquisition;
}

state actions::stop_acquisition(status &global_status)
{
    global_status.counter_restarts = 0;
    global_status.counter_resets = 0;

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

state actions::read_data(status &global_status)
{
    bool is_error = false;

    // Gather data from the available digitizers
    for (auto value = global_status.digitizers_user_ids.begin(); value != global_status.digitizers_user_ids.end(); ++value)
    {
        const unsigned int digitizer_index = value->first;
        const unsigned int user_id = value->second;
        auto digitizer = global_status.digitizers[digitizer_index];

        const bool is_ready = digitizer->AcquisitionReady();
        const bool is_overflow = digitizer->DataOverflow();

            absp_logger_console->info("Polling board: {} (user_id: {}, digitizer_index: {}); Acquisition ready: {}; Overflow: {};", digitizer->GetName(), user_id, digitizer_index, (is_ready ? "yes" : "no"), (is_overflow ? "yes" : "no"));

        if (is_overflow) {
            // The DRAM is full and data was probably lost and corrupted, so we
            // signal this as an error. A flush of the DMA would provide
            // corrupted data.
            digitizer->ResetOverflow();

            std::string error_string = "Data overflow in digitizer: ";
            error_string += digitizer->GetName();

            json_t *json_event_message = json_object();

            json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
            json_object_set_new_nocheck(json_event_message, "error", json_string(error_string.c_str()));

            actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

            json_decref(json_event_message);
            absp_logger_error->error("{}", error_string);

            is_error = true;
        }

        if (is_ready) {
                absp_logger_console->info("Getting waveforms from card;");

            const auto get_data_start = std::chrono::high_resolution_clock::now();

            const std::vector<size_t> event_counters = digitizer->GetEventCounters();

            for (uint8_t channel = 0; channel < (digitizer)->GetChannelsNumber(); channel += 1) {
                const uint8_t global_channel = channel + user_id * (digitizer)->GetChannelsNumber();
                global_status.ICR_curr_counts[global_channel] = event_counters[channel];
            }

            std::vector<struct event_waveform> waveforms;

            const int retval = digitizer->GetWaveformsFromCard(waveforms);

            const auto get_data_end = std::chrono::high_resolution_clock::now();
            auto delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(get_data_end - get_data_start);

            unsigned int waveforms_size = waveforms.size();

                absp_logger_console->info("Waveforms download: {}; waveforms number: {}; Time required: {} ms;", (retval == DIGITIZER_SUCCESS ? "success" : "failure"), waveforms_size, delta_time.count());

            if (retval == DIGITIZER_FAILURE) {
                std::string error_string = "Data fetch failure in digitizer: ";
                error_string += digitizer->GetName();

                json_t *json_event_message = json_object();

                json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
                json_object_set_new_nocheck(json_event_message, "error", json_string(error_string.c_str()));

                actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

                json_decref(json_event_message);
                absp_logger_error->error("{}", error_string);

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

                        absp_logger_console->trace("Storing waveform in buffer; Waveform index: {}; channel: {}; samples: {}; buffer pointer: {}; Waveforms buffer size: {}; Waveform size: {};", waveform_index, (unsigned int)this_waveform.channel, (unsigned int)this_waveform.samples_number, static_cast<void*>(this_waveform.buffer), current_waveform_buffer_size, this_waveform_size);

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

        absp_logger_console->info("Waveforms buffer size: {} ({} B);", global_status.waveforms_buffer_size_Number, waveforms_buffer_size_Bytes);

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

state actions::acquisition_publish_status(status &global_status)
{
        absp_logger_console->info("Publishing status;");

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
                const double ICR_rate = (global_status.ICR_curr_counts[i] - global_status.ICR_prev_counts[i]) / pubtime;

                json_array_append_new(rates, json_real(rate));
                json_array_append_new(ICR_rates, json_real(ICR_rate));
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
            const unsigned int ICR_channel_counts = global_status.ICR_curr_counts[i];

            json_array_append_new(counts, json_integer(channel_counts));
            json_array_append_new(ICR_counts, json_integer(ICR_channel_counts));
        }

        json_object_set_new_nocheck(acquisition, "rates", rates);
        json_object_set_new_nocheck(acquisition, "ICR_rates", ICR_rates);

        json_object_set_new_nocheck(acquisition, "counts", counts);
        json_object_set_new_nocheck(acquisition, "ICR_counts", ICR_counts);
    }

    json_object_set_new_nocheck(status_message, "acquisition", acquisition);
    json_object_set_new_nocheck(status_message, "digitizer", digitizer);
    json_object_set_new_nocheck(status_message, "config_file", json_string(global_status.config_filename.c_str()));

    actions::generic::publish_message(global_status, defaults_abcd_status_topic, status_message);

    json_decref(status_message);

    // Clear event partial counts
    for (unsigned int i = 0; i < global_status.partial_counts.size(); i++)
    {
        global_status.partial_counts[i] = 0;
    }
    for (unsigned int i = 0; i < global_status.ICR_prev_counts.size(); i++)
    {
        global_status.ICR_prev_counts[i] = global_status.ICR_curr_counts[i];
    }

    return states::acquisition_receive_commands;
}

state actions::restart_publish_events(status &global_status)
{
    actions::generic::publish_events(global_status);

    return states::restart_stop_acquisition;
}

state actions::restart_stop_acquisition(status &global_status)
{
    global_status.counter_restarts += 1;

    actions::generic::stop_acquisition(global_status);

    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string("Stop acquisition for a restart"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    // Normally after an error we would try to completely reset the digitizers,
    // but the support from SP Devices suggests to try to simply restart the
    // acquisition.
    // Sometimes this ends up in an infinite loop of restarts, we then try a
    // reset of the digitizers. If it is not enough we try a hard reset of the
    // control unit.
    if (global_status.counter_restarts >= defaults_absp_counter_restarts_max) {
        return states::restarts_error;
    } else {
        return states::start_acquisition;
    }
}

state actions::restart_clear_memory(status &global_status)
{
    global_status.counter_restarts = 0;

    actions::generic::clear_memory(global_status);

    return states::restart_destroy_digitizer;
}

state actions::restart_destroy_digitizer(status &global_status)
{
    global_status.counter_resets += 1;

    actions::generic::destroy_digitizer(global_status);

    if (global_status.counter_resets >= defaults_absp_counter_resets_max) {
        return states::resets_error;
    } else {
        return states::restart_create_digitizer;
    }
}

state actions::restart_destroy_control_unit(status &global_status)
{
    global_status.counter_resets = 0;

    actions::generic::destroy_control_unit(global_status);

    return states::restart_create_control_unit;
}

state actions::restart_create_control_unit(status &global_status)
{
    const bool success = actions::generic::create_control_unit(global_status);

    if (success)
    {
        return states::restart_create_digitizer;
    }
    else
    {
        return states::close_sockets;
    }
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

state actions::clear_memory(status &global_status)
{
    actions::generic::clear_memory(global_status);

        absp_logger_console->info("Clearing the json configuration;");

    // Remember to clean up the json configuration
    json_decref(global_status.config);
    global_status.config = NULL;

    return states::destroy_digitizer;
}

state actions::destroy_digitizer(status &global_status)
{
    actions::generic::destroy_digitizer(global_status);

    return states::destroy_control_unit;
}

state actions::destroy_control_unit(status &global_status)
{
    actions::generic::destroy_control_unit(global_status);

    return states::close_sockets;
}

state actions::close_sockets(status &global_status)
{
    //std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));
    struct timespec zmq_delay;
    zmq_delay.tv_sec = defaults_abcd_zmq_delay / 1000;
    zmq_delay.tv_nsec = (defaults_abcd_zmq_delay % 1000) * 1000000L;
    nanosleep(&zmq_delay, NULL);

    void *status_socket = global_status.status_socket;
    void *data_socket = global_status.data_output_socket;
    void *commands_socket = global_status.commands_socket;

    const int s = zmq_close(status_socket);
    if (s != 0)
    {
        absp_logger_error->error("ZeroMQ Error on status socket close: {}", zmq_strerror(errno));
    }

    const int d = zmq_close(data_socket);
    if (d != 0)
    {
        absp_logger_error->error("ZeroMQ Error on data socket close: {}", zmq_strerror(errno));
    }

    const int c = zmq_close(commands_socket);
    if (c != 0)
    {
        absp_logger_error->error("ZeroMQ Error on commands socket close: {}", zmq_strerror(errno));
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
        absp_logger_error->error("ZeroMQ Error on context destroy: {}", zmq_strerror(errno));
    }

    return states::stop;
}

state actions::stop(status&)
{
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

    return states::destroy_digitizer;
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
    const std::string event_message = "Acquistion error (restart: " \
                                      + std::to_string(global_status.counter_restarts) \
                                      + ", reset: " \
                                      + std::to_string(global_status.counter_resets) \
                                      + ")";

    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string(event_message.c_str()));

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

state actions::restart_digitizer_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Restart digitizer error"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::restart_clear_memory;
}

state actions::restarts_error(status &global_status)
{
    const std::string event_message = "Restarts error (too many restarts: " + std::to_string(global_status.counter_restarts) + ")";

    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string(event_message.c_str()));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::restart_clear_memory;
}

state actions::resets_error(status &global_status)
{
    const std::string event_message = "Resets error (too many resets: " + std::to_string(global_status.counter_resets) + ")";

    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string(event_message.c_str()));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::restart_destroy_control_unit;
}
