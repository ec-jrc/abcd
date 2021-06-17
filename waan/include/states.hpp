/*
 * (C) Copyright 2021, European Union, Cristiano Lino Fontana
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

#ifndef __STATES_HPP__
#define __STATES_HPP__ 1

#include "typedefs.hpp"
#include "actions.hpp"

namespace states
{
    // As a convention, initialization states are 1xx
    extern const state START;
    extern const state CREATE_CONTEXT;
    extern const state CREATE_SOCKETS;
    extern const state BIND_SOCKETS;
    extern const state READ_CONFIG;

    // Normal states are 2xx
    extern const state APPLY_CONFIG;
    extern const state PUBLISH_STATUS;
    extern const state RECEIVE_COMMANDS;
    extern const state READ_SOCKET;
    extern const state PUBLISH_DATA;

    // Closing states are 8xx
    extern const state CLEAR_MEMORY;
    extern const state CLOSE_SOCKETS;
    extern const state DESTROY_CONTEXT;
    extern const state STOP;

    // Error states are 9xx
    extern const state COMMUNICATION_ERROR;
    extern const state PARSE_ERROR;
    extern const state CONFIGURE_ERROR;
}

#endif
