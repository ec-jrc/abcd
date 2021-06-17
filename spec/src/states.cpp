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

// Normal states are 2xx
const state states::PUBLISH_STATUS = \
    {201, "Publish status", actions::publish_status };
const state states::RECEIVE_COMMANDS = \
    {202, "Receive commands", actions::receive_commands };
const state states::READ_SOCKET = \
    {203, "Read socket and preprocess", actions::read_socket };
const state states::PUBLISH_DATA = \
    {204, "Publish data", actions::publish_data };

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
