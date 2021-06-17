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
    extern const state INITIAL_CLEAR_MEMORY;
    extern const state APPLY_CONFIG;

    // Normal states are 2xx
    extern const state ACCUMULATE_PUBLISH_STATUS;
    extern const state ACCUMULATE_READ_SOCKET;
    extern const state NORMAL_PUBLISH_STATUS;
    extern const state NORMAL_READ_SOCKET;
    extern const state NORMAL_FIT_PEAK;

    // Closing states are 8xx
    extern const state CLOSE_SOCKETS;
    extern const state DESTROY_CONTEXT;
    extern const state STOP;

    // Error states are 9xx
    extern const state COMMUNICATION_ERROR;
}

#endif
