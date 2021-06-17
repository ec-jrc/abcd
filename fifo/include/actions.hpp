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
    }

    state start(status&);
    state create_context(status&);
    state create_sockets(status&);
    state bind_sockets(status&);
    state read_config(status&);

    state apply_config(status&);
    state publish_status(status&);
    state receive_commands(status&);
    state read_socket(status&);
    state publish_data(status&);

    state close_sockets(status&);
    state destroy_context(status&);
    state stop(status&);

    state communication_error(status&);
}

#endif
