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
    extern const state start;
    extern const state create_context;
    extern const state create_sockets;
    extern const state bind_sockets;
    extern const state read_config;
    extern const state create_digitizer;
    extern const state recreate_digitizer;
    extern const state configure_digitizer;
    extern const state allocate_memory;
    extern const state reconfigure_clear_memory;
    extern const state reconfigure_destroy_digitizer;

    // Normal states are 2xx
    extern const state receive_commands;
    extern const state publish_status;
    extern const state start_acquisition;
    extern const state stop_acquisition;

    // Acquisition states are 3xx
    extern const state acquisition_receive_commands;
    extern const state poll_digitizer;
    extern const state read_data;
    extern const state publish_events;
    extern const state acquisition_publish_status;
    extern const state stop_publish_events;

    // Acquisition restart states are 4xx
    extern const state restart_publish_events;
    extern const state restart_stop_acquisition;
    extern const state restart_clear_memory;
    extern const state restart_destroy_digitizer;
    extern const state restart_create_digitizer;
    extern const state restart_configure_digitizer;
    extern const state restart_allocate_memory;

    // Closing states are 8xx
    extern const state clear_memory;
    extern const state destroy_digitizer;
    extern const state close_sockets;
    extern const state destroy_context;
    extern const state stop;

    // Error states are 9xx
    extern const state communication_error;
    extern const state parse_error;
    extern const state configure_error;
    extern const state digitizer_error;
    extern const state acquisition_error;
    extern const state restart_configure_error;
}

#endif
