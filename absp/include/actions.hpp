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

#ifndef __ACTIONS_HPP__
#define __ACTIONS_HPP__ 1

#include <jansson.h>

#include "states.hpp"

namespace actions
{
    namespace generic
    {
        // This function is used in the two publish_events actions
        void publish_events(status&);
        // This function is used in the publish_status actions
        void publish_message(status&, std::string, json_t*);

        bool create_control_unit(status&);
        bool destroy_control_unit(status&);

        bool create_digitizer(status&);
        bool configure_digitizer(status&);
        void destroy_digitizer(status&);
        bool allocate_memory(status&);
        void clear_memory(status&);

        int start_acquisition(status&, unsigned int);
        void rearm_trigger(status&, unsigned int);
        void stop_acquisition(status&);
    }

    state start(status&);
    state create_context(status&);
    state create_sockets(status&);
    state bind_sockets(status&);

    state create_control_unit(status&);
    state create_digitizer(status&);
    state reconfigure_create_digitizer(status&);
    state read_config(status&);
    state configure_digitizer(status&);
    state allocate_memory(status&);

    state reconfigure_clear_memory(status&);
    state reconfigure_destroy_digitizer(status&);

    state receive_commands(status&);
    state publish_status(status&);
    state start_acquisition(status&);
    state acquisition_receive_commands(status&);
    state read_data(status&);
    state publish_events(status&);
    state acquisition_publish_status(status&);
    state stop_publish_events(status&);
    state stop_acquisition(status&);
    state clear_memory(status&);
    state destroy_digitizer(status&);
    state destroy_control_unit(status&);

    state restart_publish_events(status&);
    state restart_stop_acquisition(status&);
    state restart_clear_memory(status&);
    state restart_destroy_digitizer(status&);
    state restart_destroy_control_unit(status&);
    state restart_create_control_unit(status&);
    state restart_create_digitizer(status&);
    state restart_configure_digitizer(status&);
    state restart_allocate_memory(status&);

    state close_sockets(status&);
    state destroy_context(status&);
    state stop(status&);

    state communication_error(status&);
    state parse_error(status&);
    state configure_error(status&);
    state digitizer_error(status&);
    state acquisition_error(status&);
    state restart_configure_error(status&);
    state restart_digitizer_error(status&);
    state restarts_error(status&);
    state resets_error(status&);
}

#endif
