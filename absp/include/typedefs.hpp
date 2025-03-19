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
// For std::pair
#include <utility>

#include <jansson.h>
#include <zmq.h>

#include "defaults.h"

#include "Digitizer.hpp"

#include "LuaManager.hpp"

#define SCRIPT_WHEN_PRE 0
#define SCRIPT_WHEN_POST 1

struct status
{
    unsigned int base_period;
    std::string status_address;
    std::string data_address;
    std::string commands_address;
    
    void *context;
    void *status_socket;
    void *data_socket;
    void *commands_socket;
    
    unsigned int verbosity;
    unsigned long int status_msg_ID;
    unsigned long int data_msg_ID;

    bool identification_only;
    
    json_t *config;

    std::map<std::pair<unsigned int, unsigned int>, std::string> user_scripts;
    
    // -------------------------------------------------------------------------
    //  Digitizer specific variables
    // -------------------------------------------------------------------------
    void *adq_cu_ptr;
    std::vector<ABCD::Digitizer*> digitizers;

    std::map<unsigned int, unsigned int> digitizers_user_ids;

    unsigned int channels_number;

    // -------------------------------------------------------------------------
    //  DAQ specific variables
    // -------------------------------------------------------------------------
    std::string config_file;
    
    std::chrono::time_point<std::chrono::system_clock> start_time;
    std::chrono::time_point<std::chrono::system_clock> stop_time;
    std::chrono::time_point<std::chrono::system_clock> last_publication;
    
    std::vector<unsigned long> counts;
    std::vector<unsigned long> partial_counts;
    std::vector<size_t> ICR_prev_counts;
    std::vector<size_t> ICR_curr_counts;

    unsigned long waveforms_buffer_size_max_Number;
    unsigned long waveforms_buffer_size_Number;

    // We are using a vector because it guarantees that the buffer is contiguous.
    std::vector<uint8_t> waveforms_buffer;

    unsigned long counter_restarts;
    unsigned long counter_resets;

    LuaManager lua_manager;
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

typedef std::chrono::duration<int64_t, std::ratio<1, 1000000000ULL> > nanoseconds;

#endif
