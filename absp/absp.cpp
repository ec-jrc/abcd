/*
 * (C) Copyright 2016, 2021, European Union, Cristiano Lino Fontana
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
// For nanosleep()
#include <ctime>

#include <map>
#include <iostream>
#include <chrono>
// For std::this_thread::sleep_for
#include <thread>

#include <lua.hpp>

extern "C" {
#include "defaults.h"
#include "utilities_functions.h"
}

#include "ADQ_utilities.hpp"
#include "LuaManager.hpp"

#include "states.hpp"

#define BUFFER_SIZE 32

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
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);

        std::cout << '[' << time_buffer << "] Signal: ";

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

void print_usage(const std::string &name = std::string("abcdrp")) {
    std::cout << "Usage: " << name << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Data acquisition software that reads data from teledyne digitizers." << std::endl;
    std::cout << std::endl;
    std::cout << "Optional arguments:" << std::endl;
    std::cout << "\t-h: Display this message" << std::endl;
    std::cout << "\t-I: Digitizers identification only" << std::endl;
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
    std::cout << "\t-v: Set verbose execution" << std::endl;
    std::cout << "\t-V: Set more verbose execution" << std::endl;

    return;
}

int main(int argc, char *argv[])
{
    // Show "splash screen"
    std::cout << std::endl;
    std::cout << "===================================================" << std::endl;
    std::cout << " A.B.SP software - v. 0.1                          " << std::endl;
    std::cout << " Acquisition and Broadcast for teledyne digitizers " << std::endl;
    std::cout << "===================================================" << std::endl;
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

    bool identification_only = false;

    int c = 0;
    while ((c = getopt(argc, argv, "hIS:D:C:f:T:vV")) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'I':
                identification_only = true;
                break;
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
                } catch (std::logic_error& e) {
                }
                break;
            case 'v':
                verbosity = 1;
                break;
            case 'V':
                verbosity = 2;
                break;
            default:
                std::cout << "Unknown command: " << c << std::endl;
                break;
        }
    }

    status global_status;

    global_status.verbosity = verbosity;
    global_status.base_period = base_period;
    global_status.config = NULL;
    global_status.config_file = config_file;
    global_status.status_address = status_address;
    global_status.data_address = data_address;
    global_status.commands_address = commands_address;
    global_status.identification_only = identification_only;
    global_status.adq_cu_ptr = NULL;

    if (global_status.verbosity > 0) {
        std::cout << "Status socket address: " << status_address << std::endl;
        std::cout << "Data socket address: " << data_address << std::endl;
        std::cout << "Commands socket address: " << commands_address << std::endl;
        std::cout << "Digitizer configuration file: " << config_file << std::endl;
        std::cout << "Verbosity: " << verbosity << std::endl;
        std::cout << "Base period: " << base_period << std::endl;
        if (identification_only) {
            std::cout << WRITE_YELLOW << "WARNING" << WRITE_NC << ": Identification only, the program will quit afterwards" << std::endl;
        }
    }

    state current_state = states::start;

    // Flag for interrupting execution
    bool stop_execution = false;

    while (!stop_execution)
    {
        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Current state (" << current_state.ID << "): ";
            std::cout << current_state.description;
            std::cout << std::endl;
        }

        if (terminate_flag)
        {
            current_state = states::clear_memory;
            // If we do not clear the flag, we would enter in an infinite loop
            terminate_flag = false;
        }

        if (current_state == states::stop)
            stop_execution = true;

        // Storing this information here because they will change during the
        // state action, but for the post script they need to remain.
        const unsigned int current_state_ID = current_state.ID;
        const std::string current_state_description = current_state.description;

        try {
            const std::pair<unsigned int, unsigned int> script_key_pre(current_state_ID, SCRIPT_WHEN_PRE);
            const std::string script_source_pre = global_status.user_scripts.at(script_key_pre);

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Running pre script; ";
                std::cout << std::endl;
            }

            global_status.lua_manager.run_script(current_state_ID,
                                                 current_state_description,
                                                 "pre",
                                                 script_source_pre);

        } catch (...) {}

        current_state = current_state.act(global_status);

        try {
            const std::pair<unsigned int, unsigned int> script_key_post(current_state_ID, SCRIPT_WHEN_POST);
            const std::string script_source_post = global_status.user_scripts.at(script_key_post);

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Running post script; ";
                std::cout << std::endl;
            }

            global_status.lua_manager.run_script(current_state_ID,
                                                 current_state_description,
                                                 "post",
                                                 script_source_post);

        } catch (...) {}

        //std::this_thread::sleep_for(std::chrono::milliseconds(base_period));
        struct timespec base_delay;
        base_delay.tv_sec = base_period / 1000;
        base_delay.tv_nsec = (base_period % 1000) * 1000000L;
        nanosleep(&base_delay, NULL);
    }

    return 0;
}
