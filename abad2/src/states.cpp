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
const state states::READ_CONFIG = \
    {104, "Read configuration", actions::read_config };
const state states::CREATE_DIGITIZER = \
    {105, "Create digitizer", actions::create_digitizer };
const state states::RECREATE_DIGITIZER = \
    {106, "Create digitizer", actions::recreate_digitizer };
const state states::CONFIGURE_DIGITIZER = \
    {107, "Configure digitizer", actions::configure_digitizer };
const state states::ALLOCATE_MEMORY = \
    {108, "Allocate memory", actions::allocate_memory };
const state states::RECONFIGURE_CLEAR_MEMORY = \
    {109, "Reconfigure destroy digitizer", actions::reconfigure_clear_memory };
const state states::RECONFIGURE_DESTROY_DIGITIZER = \
    {110, "Reconfigure destroy digitizer", actions::reconfigure_destroy_digitizer };

// Normal states are 2xx
const state states::RECEIVE_COMMANDS = \
    {201, "Receive commands", actions::receive_commands };
const state states::PUBLISH_STATUS = \
    {202, "Publish status", actions::publish_status };
const state states::START_ACQUISITION = \
    {203, "Start acquisition", actions::start_acquisition };
const state states::STOP_ACQUISITION = \
    {204, "Stop acquisition", actions::stop_acquisition };

// Acquisition states are 3xx
const state states::ACQUISITION_RECEIVE_COMMANDS = \
    {301, "Acquisition receive commands", actions::acquisition_receive_commands };
const state states::ADD_TO_BUFFER = \
    {303, "Read and add to events buffer", actions::add_to_buffer };
const state states::PUBLISH_EVENTS = \
    {304, "Publish events", actions::publish_events };
const state states::ACQUISITION_PUBLISH_STATUS = \
    {305, "Acquisition publish status", actions::acquisition_publish_status };
const state states::STOP_PUBLISH_EVENTS = \
    {306, "Publish events", actions::stop_publish_events };

// Acquisition restart states are 4xx
const state states::RESTART_PUBLISH_EVENTS = \
    {401, "Restart publish events", actions::restart_publish_events};
const state states::RESTART_STOP_ACQUISITION = \
    {402, "Restart stop acquisition", actions::restart_stop_acquisition};
const state states::RESTART_CLEAR_MEMORY = \
    {403, "Restart clear memory", actions::restart_clear_memory};
const state states::RESTART_DESTROY_DIGITIZER = \
    {404, "Restart destroy digitizer", actions::restart_destroy_digitizer};
const state states::RESTART_CREATE_DIGITIZER = \
    {405, "Restart create digitizer", actions::restart_create_digitizer};
const state states::RESTART_CONFIGURE_DIGITIZER = \
    {406, "Restart configure digitizer", actions::restart_configure_digitizer};
const state states::RESTART_ALLOCATE_MEMORY = \
    {407, "Restart configure digitizer", actions::restart_allocate_memory};

// Closing states are 8xx
const state states::CLEAR_MEMORY = \
    {801, "Destroy digitizer object", actions::clear_memory };
const state states::DESTROY_DIGITIZER = \
    {802, "Destroy digitizer object", actions::destroy_digitizer };
const state states::CLOSE_SOCKETS = \
    {803, "Close sockets", actions::close_sockets };
const state states::DESTROY_CONTEXT = \
    {804, "Destroy ZeroMQ context", actions::destroy_context };
const state states::STOP = \
    {899, "Stop", actions::stop };

// Error states are 9xx
const state states::COMMUNICATION_ERROR = \
    {901, "Communication error", actions::communication_error };
const state states::PARSE_ERROR = \
    {902, "Config parse error", actions::parse_error };
const state states::DIGITIZER_ERROR = \
    {903, "Digitizer error", actions::digitizer_error };
const state states::CONFIGURE_ERROR = \
    {903, "Configure error", actions::configure_error };
const state states::ACQUISITION_ERROR = \
    {904, "Acquisition error", actions::acquisition_error };
const state states::RESTART_CONFIGURE_ERROR = \
    {905, "Restart configure error", actions::restart_configure_error };
