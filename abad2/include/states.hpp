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
    extern const state CREATE_DIGITIZER;
    extern const state RECREATE_DIGITIZER;
    extern const state CONFIGURE_DIGITIZER;
    extern const state ALLOCATE_MEMORY;
    extern const state RECONFIGURE_CLEAR_MEMORY;
    extern const state RECONFIGURE_DESTROY_DIGITIZER;

    // Normal states are 2xx
    extern const state RECEIVE_COMMANDS;
    extern const state PUBLISH_STATUS;
    extern const state START_ACQUISITION;
    extern const state STOP_ACQUISITION;

    // Acquisition states are 3xx
    extern const state ACQUISITION_RECEIVE_COMMANDS;
    extern const state POLL_DIGITIZER;
    extern const state ADD_TO_BUFFER;
    extern const state PUBLISH_EVENTS;
    extern const state ACQUISITION_PUBLISH_STATUS;
    extern const state STOP_PUBLISH_EVENTS;

    // Acquisition restart states are 4xx
    extern const state RESTART_PUBLISH_EVENTS;
    extern const state RESTART_STOP_ACQUISITION;
    extern const state RESTART_CLEAR_MEMORY;
    extern const state RESTART_DESTROY_DIGITIZER;
    extern const state RESTART_CREATE_DIGITIZER;
    extern const state RESTART_CONFIGURE_DIGITIZER;
    extern const state RESTART_ALLOCATE_MEMORY;

    // Closing states are 8xx
    extern const state CLEAR_MEMORY;
    extern const state DESTROY_DIGITIZER;
    extern const state CLOSE_SOCKETS;
    extern const state DESTROY_CONTEXT;
    extern const state STOP;

    // Error states are 9xx
    extern const state COMMUNICATION_ERROR;
    extern const state PARSE_ERROR;
    extern const state CONFIGURE_ERROR;
    extern const state DIGITIZER_ERROR;
    extern const state ACQUISITION_ERROR;
    extern const state RESTART_CONFIGURE_ERROR;
}

#endif
