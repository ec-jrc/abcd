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
    extern const state CLOSE_SOCKETS;
    extern const state DESTROY_CONTEXT;
    extern const state STOP;

    // Error states are 9xx
    extern const state COMMUNICATION_ERROR;
}

#endif
