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
        void clear_memory(status&);
        bool publish_status(status&);
        bool read_socket(status&);
    }

    state start(status&);
    state create_context(status&);
    state create_sockets(status&);
    state bind_sockets(status&);
    state read_config(status&);
    state initial_clear_memory(status&);
    state apply_config(status&);

    state accumulate_publish_status(status&);
    state accumulate_read_socket(status&);
    state normal_publish_status(status&);
    state normal_read_socket(status&);
    state normal_fit_peak(status&);

    state close_sockets(status&);
    state destroy_context(status&);
    state stop(status&);

    state communication_error(status&);
}

#endif
