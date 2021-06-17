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

#include <jansson.h>
#include <zmq.h>

#include "defaults.h"
#include "class_caen_hv.h"

struct status
{
    std::string status_address = defaults_hijk_status_address;
    std::string commands_address = defaults_hijk_commands_address;

    void *context = nullptr;
    void *status_socket = nullptr;
    void *commands_socket = nullptr;

    unsigned int verbosity = 0;
    unsigned long int status_msg_ID = 0;

    std::string config_file = defaults_hijk_config_file;
    json_t *config = nullptr;

    unsigned int connection_type = defaults_hijk_connection_type;
    unsigned int link_number = defaults_hijk_link_number;
    unsigned int CONET_node = defaults_hijk_CONET_node;
    unsigned int VME_address = defaults_hijk_VME_address;
    int model = defaults_hijk_model;

    CAENHV *hv_card = nullptr;
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
