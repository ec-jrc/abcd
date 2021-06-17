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
        // This function is used in the publish_status actions
        void publish_message(status&, std::string, json_t*);
        void close_file(status&);
    }

    state start(status&);
    state create_context(status&);
    state create_sockets(status&);
    state bind_sockets(status&);

    state publish_status(status&);
    state empty_queue(status&);
    state receive_commands(status&);

    state open_file(status&);
    state write_data(status&);
    state flush_file(status&);
    state saving_publish_status(status&);
    state saving_receive_commands(status&);
    state close_file(status&);

    state stop_close_file(status&);
    state close_sockets(status&);
    state destroy_context(status&);
    state stop(status&);

    state communication_error(status&);
    state io_error(status&);
}

#endif
