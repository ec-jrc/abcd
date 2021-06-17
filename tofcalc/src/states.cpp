// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "typedefs.hpp"
#include "actions.hpp"
#include "states.hpp"

// As a convention, initialization states are 1xx
const state states::START = \
    {100, "Start", actions::start };
const state states::CREATE_CONTEXT = \
    {101, "Create ZeroMQ context", actions::create_context };
const state states::CREATE_SOCKETS = \
    {102, "Create sockets", actions::create_sockets };
const state states::BIND_SOCKETS = \
    {103, "Bind sockets", actions::bind_sockets };
const state states::READ_CONFIG = \
    {104, "Read configuration", actions::read_config };

// Normal states are 2xx
const state states::APPLY_CONFIG = \
    {201, "Apply configuration", actions::apply_config };
const state states::PUBLISH_STATUS = \
    {202, "Publish status", actions::publish_status };
const state states::RECEIVE_COMMANDS = \
    {203, "Receive commands", actions::receive_commands };
const state states::READ_SOCKET = \
    {204, "Read socket and preprocess", actions::read_socket };
const state states::PUBLISH_DATA = \
    {205, "Publish data", actions::publish_data };

// Closing states are 8xx
const state states::CLOSE_SOCKETS = \
    {801, "Close sockets", actions::close_sockets };
const state states::DESTROY_CONTEXT = \
    {802, "Destroy ZeroMQ context", actions::destroy_context };
const state states::STOP = \
    {899, "Stop", actions::stop };

// Error states are 9xx
const state states::COMMUNICATION_ERROR = \
    {901, "Communication error", actions::communication_error };
