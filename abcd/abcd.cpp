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

// For getopt()
#include <unistd.h>
#include <csignal>
// For memcpy()
#include <cstring>

#include <map>
#include <iostream>
#include <chrono>
// For std::this_thread::sleep_for
#include <thread>

#include <zmq.h>
#include <json/json.h>

#include "utilities_functions.hpp"
#include "socket_functions.hpp"
#include "defaults.h"
#include "states.hpp"

#include "class_caen_dgtz.h"

unsigned int verbosity = defaults_abcd_verbosity;
bool terminate_flag = false;

//! Handles standard signals.
/*! SIGTERM (from kill): terminates kindly forcing the status to the closing branch of the state machine.
    SIGINT (from ctrl-c): same behaviour as SIGTERM
    SIGHUP (from shell processes): same behaviour as SIGTERM
 */
void signal_handler(int signum)
{
    if (verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string();
        std::cout << "] Signal: ";

        if (signum == SIGINT)
            std::cout << "SIGINT" << std::endl;
        else if (signum == SIGTERM)
            std::cout << "SIGTERM" << std::endl;
        else if (signum == SIGHUP)
            std::cout << "SIGHUP" << std::endl;
        else
            std::cout << signum << std::endl;
    }

    if (signum == SIGINT || signum == SIGTERM || signum == SIGHUP)
    {
        terminate_flag = true;
    }
}

void print_usage(const std::string &name = std::string("abcd")) {
    std::cout << "Usage: " << name << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Data acquisition software that reads data from CAEN digitizers." << std::endl;
    std::cout << std::endl;
    std::cout << "Optional arguments:" << std::endl;
    std::cout << "\t-h: Display this message" << std::endl;
    std::cout << "\t-S <address>: Status socket address, default: ";
    std::cout << defaults_abcd_status_address << std::endl;
    std::cout << "\t-D <address>: Data socket address, default: ";
    std::cout << defaults_abcd_data_address << std::endl;
    std::cout << "\t-C <address>: Commands socket address, default: ";
    std::cout << defaults_abcd_commands_address << std::endl;
    std::cout << "\t-f <file_name>: Digitizer configuration file, default: ";
    std::cout << defaults_abcd_config_file << std::endl;
    std::cout << "\t-T <period>: Set base period in milliseconds, default: ";
    std::cout << defaults_abcd_base_period << std::endl;
    std::cout << "\t-c <connection_type>: Connection type, default: ";
    std::cout << defaults_abcd_connection_type << std::endl;
    std::cout << "\t-l <link_number>: Link number, default: ";
    std::cout << defaults_abcd_link_number << std::endl;
    std::cout << "\t-n <CONET_node>: Connection node, default: ";
    std::cout << defaults_abcd_CONET_node << std::endl;
    std::cout << "\t-V <VME_address>: VME address, default: ";
    std::cout << std::hex << defaults_abcd_VME_address << std::dec << std::endl;
    std::cout << "\t-B <size>: Events buffer maximum size, default: ";
    std::cout << defaults_abcd_events_buffer_max_size << std::endl;
    std::cout << "\t-p <publish_timeout>: Defines the maximum time in seconds between subsequent publications, default: ";
    std::cout << defaults_abcd_publish_timeout << std::endl;
    std::cout << "\t-v: Set verbose execution" << std::endl;

    return;
}

int main(int argc, char *argv[])
{
    // Show "splash screen"
    std::cout << std::endl;
    std::cout << "=============================================" << std::endl;
    std::cout << " A.B.C.D. software - v. 0.1                  " << std::endl;
    std::cout << " Acquisition and Broadcast of Collected Data " << std::endl;
    std::cout << "=============================================" << std::endl;
    std::cout << std::endl;

    // Register the handler for SIGTERM (from kill), SIGINT (from ctrl-c)
    signal(SIGTERM, signal_handler);  
    signal(SIGINT, signal_handler);  
    signal(SIGHUP, signal_handler);  

    std::string status_address = defaults_abcd_status_address;
    std::string data_address = defaults_abcd_data_address;
    std::string commands_address = defaults_abcd_commands_address;
    std::string config_file = defaults_abcd_config_file;
    unsigned int base_period = defaults_abcd_base_period;
    float publish_timeout = defaults_abcd_publish_timeout;

    unsigned int connection_type = defaults_abcd_connection_type;
    unsigned int link_number = defaults_abcd_link_number;
    unsigned int CONET_node = defaults_abcd_CONET_node;
    unsigned int VME_address = defaults_abcd_VME_address;
    unsigned int events_buffer_max_size = defaults_abcd_events_buffer_max_size;

    int c = 0;
    while ((c = getopt(argc, argv, "hS:D:C:f:T:c:l:n:V:B:p:v")) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'S':
                status_address = optarg;
                break;
            case 'D':
                data_address = optarg;
                break;
            case 'C':
                commands_address = optarg;
                break;
            case 'f':
                config_file = optarg;
                break;
            case 'T':
                try
                {
                    base_period = std::stoul(optarg);
                }
                catch (std::logic_error &e)
                { }
                break;
            case 'v':
                verbosity = 1;
                break;
            case 'c':
                connection_type = std::stoul(optarg);
                break;
            case 'l':
                link_number = std::stoul(optarg);
                break;
            case 'n':
                CONET_node = std::stoul(optarg);
                break;
            case 'V':
                VME_address = std::stoul(optarg, nullptr, 0);
                break;
            case 'B':
                events_buffer_max_size = std::stoul(optarg);
                break;
            case 'p':
                publish_timeout = std::stof(optarg);
                break;
            default:
                std::cout << "Unknown command: " << c << std::endl;
                break;
        }
    }

    status global_status;

    global_status.verbosity = verbosity;
    global_status.connection_type = connection_type;
    global_status.link_number = link_number;
    global_status.CONET_node = CONET_node;
    global_status.VME_address = VME_address;
    global_status.events_buffer_max_size = events_buffer_max_size;
    global_status.config_file = config_file;
    global_status.status_address = status_address;
    global_status.data_address = data_address;
    global_status.commands_address = commands_address;
    global_status.publish_timeout = std::chrono::milliseconds(static_cast<unsigned int>(publish_timeout * 1000));

    if (global_status.verbosity > 0) {
        std::cout << "Connection type: " << connection_type << std::endl;
        std::cout << "Link number: " << link_number << std::endl;
        std::cout << "CONET node: " << CONET_node << std::endl;
        std::cout << "VME address: 0x" << std::hex << VME_address << std::dec << std::endl;
        std::cout << "Status socket address: " << status_address << std::endl;
        std::cout << "Data socket address: " << data_address << std::endl;
        std::cout << "Commands socket address: " << commands_address << std::endl;
        std::cout << "Digitizer configuration file: " << config_file << std::endl;
        std::cout << "Verbosity: " << verbosity << std::endl;
        std::cout << "Base period: " << base_period << std::endl;
        std::cout << "Publish timeout: " << publish_timeout << std::endl;
        std::cout << "Events buffer size: " << events_buffer_max_size << std::endl;
    }

    state current_state = states::START;

    // Flag for interrupting execution
    bool stop_execution = false;

    while (!stop_execution)
    {
        if (global_status.verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "Current state (" << current_state.ID << "): ";
            std::cout << current_state.description;
            std::cout << std::endl;
        }

        if (terminate_flag)
        {
            current_state = states::CLEAR_MEMORY;
            // If we do not clear the flag, we would enter in an infinite loop
            terminate_flag = false;
        }

        if (current_state == states::STOP)
            stop_execution = true;

        current_state = current_state.act(global_status);

        std::this_thread::sleep_for (std::chrono::milliseconds(base_period));
    }

    return 0;
}
