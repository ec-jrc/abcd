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

#ifndef __TYPEDEFS_HPP__
#define __TYPEDEFS_HPP__ 1

#include <unistd.h>
#include <string>
#include <chrono>
#include <queue>

#include <json/json.h>
#include <zmq.h>

#include "defaults.h"
#include "events.hpp"
#include "class_caen_dgtz.h"

struct status
{
    unsigned int base_period = defaults_abcd_base_period;
    std::string status_address = defaults_abcd_status_address;
    std::string data_address = defaults_abcd_data_address;
    std::string commands_address = defaults_abcd_commands_address;

    void *context = nullptr;
    void *status_socket = nullptr;
    void *data_socket = nullptr;
    void *commands_socket = nullptr;

    unsigned int verbosity = 0;
    size_t status_msg_ID = 0;
    size_t events_msg_ID = 0;
    size_t waveforms_msg_ID = 0;

    Json::Value config;

    CAENDgtz *digitizer = nullptr;
    unsigned int connection_type;
    unsigned int link_number;
    unsigned int CONET_node;
    unsigned int VME_address;
    unsigned int events_buffer_max_size;
    std::string config_file = defaults_abcd_config_file;

    std::chrono::time_point<std::chrono::system_clock> start_time;
    std::chrono::time_point<std::chrono::system_clock> stop_time;
    std::chrono::time_point<std::chrono::system_clock> last_publication;
    std::chrono::milliseconds publish_timeout = std::chrono::milliseconds(defaults_abcd_publish_timeout * 1000);

    char *readout_buffer = NULL;
    CAEN_DGTZ_UINT16_EVENT_t *Evt_STD = nullptr;
    CAEN_DGTZ_DPP_PSD_Event_t **Evt_PSD = nullptr;
    CAEN_DGTZ_DPP_PSD_Waveforms_t *Waveforms_PSD = nullptr;

    uint32_t *numEvents = nullptr;
    uint64_t *previous_timestamp = nullptr;
    uint64_t *time_offset = nullptr;
    uint64_t offset_step = 0x40000000;

    uint16_t dpp_version = 0;
    bool flag_tt64 = false;
    bool enabled_waveforms = false;
    bool show_gates = false;

    std::vector<unsigned long> counts;
    std::vector<unsigned long> partial_counts;
    // Incoming Counting Rate (ICR) counts from board firmware
    std::vector<unsigned long> ICR_counts;

    // We are using a vector because it guarantees that the buffer is contiguous.
    std::vector<struct event_PSD> events_buffer;
    std::vector<struct event_waveform> waveforms_buffer;
};

struct state
{
    unsigned int ID;
    const char* description;
    struct state (*act)(struct status&);
};

inline bool operator==(struct state a, struct state b)
{
    return a.ID == b.ID;
}

typedef struct status status;
typedef struct state state;
typedef struct event_PSD event_PSD;
typedef state (*action)(status);

#endif
