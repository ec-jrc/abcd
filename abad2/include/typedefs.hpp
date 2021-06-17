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
#include <vector>
#include <map>

#include <jansson.h>
#include <zmq.h>
#include <digilent/waveforms/dwf.h>

#include "defaults.h"
#include "events.hpp"

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
    unsigned long int status_msg_ID = 0;
    unsigned long int data_msg_ID = 0;

    json_t *config = nullptr;

    int device_number;
    HDWF ad2_handler = hdwfNone;

    int ad2_buffer_size = 0;

    std::vector<int> enabled_channels;
    std::map<int, double> channels_ranges;
    std::map<int, double> channels_offsets;

    unsigned int events_buffer_max_size;
    std::string config_file = defaults_abcd_config_file;

    std::chrono::time_point<std::chrono::system_clock> start_time;
    std::chrono::time_point<std::chrono::system_clock> stop_time;
    std::chrono::time_point<std::chrono::system_clock> last_publication;

    int channels_number = 0;

    std::vector<unsigned long> counts;
    std::vector<unsigned long> partial_counts;

    // We are using a vector because it guarantees that the buffer is contiguous.
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

typedef std::chrono::duration<int64_t, std::ratio<1, 1000000000ULL> > nanoseconds;

#endif
