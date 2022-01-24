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
#include <fstream>

#include <zmq.h>

#include "defaults.h"

struct status
{
    std::string status_address = defaults_lmno_status_address;
    std::string commands_address = defaults_lmno_commands_address;
    std::string abcd_data_address = defaults_abcd_data_address;
    std::string abcd_status_address = defaults_abcd_status_address;
    std::string waan_status_address = defaults_waan_status_address;

    void *context = nullptr;
    void *status_socket = nullptr;
    void *commands_socket = nullptr;
    void *abcd_data_socket = nullptr;
    void *abcd_status_socket = nullptr;
    void *waan_status_socket = nullptr;

    unsigned int verbosity = 0;
    unsigned long int status_msg_ID = 0;
    std::chrono::time_point<std::chrono::system_clock> last_publication;

    std::chrono::time_point<std::chrono::system_clock> start_time;

    std::string events_file_name;
    std::string waveforms_file_name;
    std::string raw_file_name;

    std::ofstream raw_output_file;
    std::ofstream events_output_file;
    std::ofstream waveforms_output_file;

    size_t events_file_size;
    size_t waveforms_file_size;
    size_t raw_file_size;
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
typedef state (*action)(status);

#endif