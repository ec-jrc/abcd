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

extern "C" {
#include "utilities_functions.h"
#include "socket_functions.h"
#include "defaults.h"
}

#include "states.hpp"

#define BUFFER_SIZE 32

unsigned int verbosity = defaults_lmno_verbosity;
bool terminate_flag = false;

//! Handles standard signals.
/*! SIGTERM (from kill): terminates kindly forcing the status to the closing branch of the state machine.
    SIGINT (from ctrl-c): same behaviour as SIGTERM
    SIGHUP (from shell processes): same behaviour as SIGTERM
    SIGINFO (from ctrl-t, if defined): prints the current date, time and state
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
        #ifdef SIGINFO
        else if (signum == SIGINFO)
            std::cout << "SIGINFO" << std::endl;
        #endif
        else
            std::cout << signum << std::endl;
    }

    if (signum == SIGINT || signum == SIGTERM || signum == SIGHUP)
    {
        terminate_flag = true;
    }
    #ifdef SIGINFO
    else if (signum == SIGINFO)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);

        std::cout << '[' << time_buffer << "] Running ";
        std::cout << std::endl;
    }
    #endif
}

void print_usage(const std::string &name = std::string("lmno")) {
    char abcd_data_address[sizeof("[wxyz://255.255.255.255:65535]")];
    address_bind_to_connect(abcd_data_address, sizeof(abcd_data_address), defaults_abcd_data_address, defaults_abcd_ip);
    
    char abcd_status_address[sizeof("[wxyz://255.255.255.255:65535]")];
    address_bind_to_connect(abcd_status_address, sizeof(abcd_status_address), defaults_abcd_status_address, defaults_abcd_ip);
    
    char waan_status_address[sizeof("[wxyz://255.255.255.255:65535]")];
    address_bind_to_connect(waan_status_address, sizeof(waan_status_address), defaults_waan_status_address, defaults_abcd_ip);
    
    std::cout << "Usage: " << name << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Data logging software that saved data collected with ABCD." << std::endl;
    std::cout << std::endl;
    std::cout << "Optional arguments:" << std::endl;
    std::cout << "\t-h: Display this message" << std::endl;
    std::cout << "\t-A <address>: abcd data socket address, default: " << abcd_data_address << std::endl;
    std::cout << "\t-s <address>: abcd status socket address, default: " << abcd_status_address << std::endl;
    std::cout << "\t-w <address>: waan status socket address, default: " << waan_status_address << std::endl;
    std::cout << "\t-S <address>: Status socket address, default: ";
    std::cout << defaults_lmno_status_address << std::endl;
    std::cout << "\t-C <address>: Commands socket address, default: ";
    std::cout << defaults_lmno_commands_address << std::endl;
    std::cout << "\t-T <period>: Set base period in milliseconds, default: ";
    std::cout << defaults_lmno_base_period << std::endl;
    std::cout << "\t-v: Set verbose execution" << std::endl;
    std::cout << "\t-V: Set verbose execution with more output" << std::endl;

    return;
}

int main(int argc, char *argv[])
{
    // Show "splash screen"
    std::cout << std::endl;
    std::cout << "==========================" << std::endl;
    std::cout << " Da.Sa. software - v. 0.1 " << std::endl;
    std::cout << " Data Saver module        " << std::endl;
    std::cout << "==========================" << std::endl;
    std::cout << std::endl;

    // Register the handler for SIGTERM (from kill), SIGINT (from ctrl-c) and SIGINFO (from ctrl-t)
    signal(SIGTERM, signal_handler);  
    signal(SIGINT, signal_handler);  
    signal(SIGHUP, signal_handler);

    #ifdef SIGINFO
    signal(SIGINFO, signal_handler);  
    #endif

    char str_abcd_data_address[sizeof("[wxyz://255.255.255.255:65535]")];
    address_bind_to_connect(str_abcd_data_address, sizeof(str_abcd_data_address), defaults_abcd_data_address, defaults_abcd_ip);
    
    char str_abcd_status_address[sizeof("[wxyz://255.255.255.255:65535]")];
    address_bind_to_connect(str_abcd_status_address, sizeof(str_abcd_status_address), defaults_abcd_status_address, defaults_abcd_ip);
    
    char str_waan_status_address[sizeof("[wxyz://255.255.255.255:65535]")];
    address_bind_to_connect(str_waan_status_address, sizeof(str_waan_status_address), defaults_waan_status_address, defaults_abcd_ip);
    
    std::string abcd_data_address = str_abcd_data_address;
    std::string abcd_status_address = str_abcd_status_address;
    std::string waan_status_address = str_waan_status_address;

    std::string status_address = defaults_lmno_status_address;
    std::string commands_address = defaults_lmno_commands_address;
    unsigned int base_period = defaults_lmno_base_period;

    int c = 0;
    while ((c = getopt(argc, argv, "hA:s:w:S:C:T:vV")) != -1) {
        switch (c) {
            case 'h':
                print_usage(std::string(argv[0]));
                return EXIT_SUCCESS;
            case 'A':
                abcd_data_address = optarg;
                break;
            case 's':
                abcd_status_address = optarg;
                break;
            case 'w':
                waan_status_address = optarg;
                break;
            case 'S':
                status_address = optarg;
                break;
            case 'C':
                commands_address = optarg;
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
    global_status.abcd_data_address = abcd_data_address;
    global_status.abcd_status_address = abcd_status_address;
    global_status.waan_status_address = waan_status_address;
    global_status.status_address = status_address;
    global_status.commands_address = commands_address;

    if (global_status.verbosity > 0) {
        std::cout << "abcd data socket address: " << abcd_data_address << std::endl;
        std::cout << "abcd status socket address: " << abcd_status_address << std::endl;
        std::cout << "waan status socket address: " << waan_status_address << std::endl;
        std::cout << "Status socket address: " << status_address << std::endl;
        std::cout << "Commands socket address: " << commands_address << std::endl;
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
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Current state (" << current_state.ID << "): ";
            std::cout << current_state.description;
            std::cout << std::endl;
        }

        if (terminate_flag)
        {
            current_state = states::STOP_CLOSE_FILE;
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
