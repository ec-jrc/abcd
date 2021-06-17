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
// vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4

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
#include <jansson.h>

#include "utilities_functions.hpp"
#include "defaults.h"
#include "states.hpp"

#include "class_caen_hv.h"

unsigned int verbosity = defaults_hijk_verbosity;
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

void print_usage(const std::string &name = std::string("hijk")) {
    std::cout << "Usage: " << name << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "High voltages management software for CAEN HV modules." << std::endl;
    std::cout << std::endl;
    std::cout << "Optional arguments:" << std::endl;
    std::cout << "\t-h: Display this message" << std::endl;
    std::cout << "\t-S <address>: Status socket address, default: ";
    std::cout << defaults_hijk_status_address << std::endl;
    std::cout << "\t-C <address>: Commands socket address, default: ";
    std::cout << defaults_hijk_commands_address << std::endl;
    std::cout << "\t-f <file_name>: HV configuration file, default: ";
    std::cout << defaults_hijk_config_file << std::endl;
    std::cout << "\t-T <period>: Set base period in milliseconds, default: ";
    std::cout << defaults_hijk_base_period << std::endl;
    std::cout << "\t-c <connection_type>: Connection type, default: ";
    std::cout << defaults_hijk_connection_type << std::endl;
    std::cout << "\t-l <link_number>: Link number, default: ";
    std::cout << defaults_hijk_link_number << std::endl;
    std::cout << "\t-n <CONET_node>: Connection node, default: ";
    std::cout << defaults_hijk_CONET_node << std::endl;
    std::cout << "\t-V <VME_address>: VME address, default: ";
    std::cout << std::hex << defaults_hijk_VME_address << std::dec << std::endl;
    std::cout << "\t-m <model>: HV model description, default: ";
    std::cout << defaults_hijk_model << std::endl;
    std::cout << "\t-v: Set verbose execution" << std::endl;

    return;
}

int main(int argc, char *argv[])
{
    // Show "splash screen"
    std::cout << std::endl;
    std::cout << "==========================" << std::endl;
    std::cout << " Hi.Vo. software - v. 0.1 " << std::endl;
    std::cout << " High-Voltages manager    " << std::endl;
    std::cout << "==========================" << std::endl;
    std::cout << std::endl;

    // Register the handler for SIGTERM (from kill), SIGINT (from ctrl-c), SIGHUP (from tmux close)
    signal(SIGTERM, signal_handler);  
    signal(SIGINT, signal_handler);  
    signal(SIGHUP, signal_handler);

    std::string status_address = defaults_hijk_status_address;
    std::string commands_address = defaults_hijk_commands_address;
    std::string config_file = defaults_hijk_config_file;
    unsigned int base_period = defaults_hijk_base_period;

    unsigned int connection_type = defaults_hijk_connection_type;
    unsigned int link_number = defaults_hijk_link_number;
    unsigned int CONET_node = defaults_hijk_CONET_node;
    unsigned int VME_address = defaults_hijk_VME_address;

    int model = defaults_hijk_model;

    int c = 0;
    while ((c = getopt(argc, argv, "hS:C:f:T:c:l:n:V:m:v")) != -1) {
        switch (c) {
            case 'h':
                print_usage(std::string(argv[0]));
                return EXIT_SUCCESS;
            case 'S':
                status_address = optarg;
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
                // To be able to read a hex number we need to give the other two parameters to stoul
                VME_address = std::stoul(optarg, nullptr, 0);
                break;
            case 'm':
                model = std::stoi(optarg);
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
    global_status.config_file = config_file;
    global_status.status_address = status_address;
    global_status.commands_address = commands_address;
    global_status.model = model;

    if (global_status.verbosity > 0) {
        std::cout << "Model: " << model << std::endl;
        std::cout << "Connection type: " << connection_type << std::endl;
        std::cout << "Link number: " << link_number << std::endl;
        std::cout << "CONET node: " << CONET_node << std::endl;
        std::cout << "VME address: " << std::hex << VME_address << std::dec << std::endl;
        std::cout << "Status socket address: " << status_address << std::endl;
        std::cout << "Commands socket address: " << commands_address << std::endl;
        std::cout << "HV configuration file: " << config_file << std::endl;
        std::cout << "Verbosity: " << verbosity << std::endl;
        std::cout << "Base period: " << base_period << std::endl;
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
            current_state = states::DESTROY_HV;
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
