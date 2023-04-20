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
const state states::start = \
    {100, "Start", actions::start };
const state states::create_context = \
    {101, "Create ZeroMQ context", actions::create_context };
const state states::create_sockets = \
    {102, "Create sockets", actions::create_sockets };
const state states::bind_sockets = \
    {103, "Bind sockets", actions::bind_sockets };
const state states::read_config = \
    {104, "Read configuration", actions::read_config };
const state states::create_control_unit = \
    {105, "Create control unit", actions::create_control_unit };
const state states::create_digitizer = \
    {106, "Create digitizer", actions::create_digitizer };
const state states::recreate_digitizer = \
    {107, "Recreate digitizer", actions::recreate_digitizer };
const state states::configure_digitizer = \
    {108, "Configure digitizer", actions::configure_digitizer };
const state states::allocate_memory = \
    {109, "Allocate memory", actions::allocate_memory };
const state states::reconfigure_clear_memory = \
    {110, "Reconfigure destroy digitizer", actions::reconfigure_clear_memory };
const state states::reconfigure_destroy_digitizer = \
    {111, "Reconfigure destroy digitizer", actions::reconfigure_destroy_digitizer };

// Normal states are 2xx
const state states::receive_commands = \
    {201, "Receive commands", actions::receive_commands };
const state states::publish_status = \
    {202, "Publish status", actions::publish_status };
const state states::start_acquisition = \
    {203, "Start acquisition", actions::start_acquisition };
const state states::stop_acquisition = \
    {204, "Stop acquisition", actions::stop_acquisition };

// Acquisition states are 3xx
const state states::acquisition_receive_commands = \
    {301, "Acquisition receive commands", actions::acquisition_receive_commands };
const state states::read_data = \
    {303, "Read and add to events buffer", actions::read_data };
const state states::publish_events = \
    {304, "Publish events", actions::publish_events };
const state states::acquisition_publish_status = \
    {305, "Acquisition publish status", actions::acquisition_publish_status };
const state states::stop_publish_events = \
    {306, "Publish events", actions::stop_publish_events };

// Acquisition restart states are 4xx
const state states::restart_publish_events = \
    {401, "Restart publish events", actions::restart_publish_events};
const state states::restart_stop_acquisition = \
    {402, "Restart stop acquisition", actions::restart_stop_acquisition};
const state states::restart_clear_memory = \
    {403, "Restart clear memory", actions::restart_clear_memory};
const state states::restart_destroy_digitizer = \
    {404, "Restart destroy digitizer", actions::restart_destroy_digitizer};
const state states::restart_create_digitizer = \
    {405, "Restart create digitizer", actions::restart_create_digitizer};
const state states::restart_configure_digitizer = \
    {406, "Restart configure digitizer", actions::restart_configure_digitizer};
const state states::restart_allocate_memory = \
    {407, "Restart configure digitizer", actions::restart_allocate_memory};

// Closing states are 8xx
const state states::clear_memory = \
    {801, "Clearing memory", actions::clear_memory };
const state states::destroy_digitizer = \
    {802, "Destroy digitizer object", actions::destroy_digitizer };
const state states::destroy_control_unit = \
    {803, "Destroy control_unit object", actions::destroy_control_unit };
const state states::close_sockets = \
    {804, "Close sockets", actions::close_sockets };
const state states::destroy_context = \
    {805, "Destroy ZeroMQ context", actions::destroy_context };
const state states::stop = \
    {899, "Stop", actions::stop };

// Error states are 9xx
const state states::communication_error = \
    {901, "Communication error", actions::communication_error };
const state states::parse_error = \
    {902, "Config parse error", actions::parse_error };
const state states::digitizer_error = \
    {903, "Digitizer error", actions::digitizer_error };
const state states::configure_error = \
    {903, "Configure error", actions::configure_error };
const state states::acquisition_error = \
    {904, "Acquisition error", actions::acquisition_error };
const state states::restart_configure_error = \
    {905, "Restart configure error", actions::restart_configure_error };
