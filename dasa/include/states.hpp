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

    // Normal states are 2xx
    extern const state PUBLISH_STATUS;
    extern const state EMPTY_QUEUE;
    extern const state RECEIVE_COMMANDS;

    // Saving states are 3xx
    extern const state OPEN_FILE;
    extern const state WRITE_DATA;
    extern const state FLUSH_FILE;
    extern const state SAVING_PUBLISH_STATUS;
    extern const state SAVING_RECEIVE_COMMANDS;
    extern const state CLOSE_FILE;

    // Closing states are 8xx
    extern const state STOP_CLOSE_FILE;
    extern const state CLOSE_SOCKETS;
    extern const state DESTROY_CONTEXT;
    extern const state STOP;

    // Error states are 9xx
    extern const state COMMUNICATION_ERROR;
    extern const state IO_ERROR;
}

#endif
