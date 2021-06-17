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

extern "C" {
#include <zmq.h>
#include <jansson.h>

#include "utilities_functions.h"
}

#include "defaults.h"
#include "states.hpp"

#define BUFFER_SIZE 32


unsigned int verbosity = defaults_tofcalc_verbosity;
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

        std::cout << '[' << time_buffer << "] Running" << std::endl;
    }
    #endif
}

void print_usage(const std::string &name = std::string("spec")) {
    char data_address[sizeof("[wxyz://255.255.255.255:65535]")];
    address_bind_to_connect(data_address, sizeof(data_address), defaults_abcd_data_address, defaults_abcd_ip);

    std::cout << "Usage: " << name << " [options]" << std::endl;
    std::cout << "\t-h: Display this message" << std::endl;
    std::cout << "\t-A <address>: ABCD data socket address, default: " << data_address << std::endl;
    std::cout << "\t-S <address>: Status socket address, default: ";
    std::cout << defaults_tofcalc_status_address << std::endl;
    std::cout << "\t-D <address>: Data socket address, default: ";
    std::cout << defaults_tofcalc_data_address << std::endl;
    std::cout << "\t-C <address>: Commands socket address, default: ";
    std::cout << defaults_tofcalc_commands_address << std::endl;
    std::cout << "\t-T <period>: Set base period in milliseconds, default: ";
    std::cout << defaults_tofcalc_base_period << std::endl;
    std::cout << "\t-n <ns_per_sample>: Set nanoseconds per time step, default: ";
    std::cout << defaults_tofcalc_ns_per_sample << std::endl;
    std::cout << "\t-f <config_file>: Set config file, default: none" << std::endl;
    std::cout << "\t-v: Set verbose execution" << std::endl;
    std::cout << "\t-V: Set verbose execution with more details" << std::endl;

    return;
}

int main(int argc, char *argv[])
{
    // Show "splash screen"
    std::cout << std::endl;
    std::cout << "===========================" << std::endl;
    std::cout << " ToFCalc software - v. 0.1 " << std::endl;
    std::cout << "===========================" << std::endl;
    std::cout << std::endl;

    // Register the handler for SIGTERM (from kill), SIGINT (from ctrl-c) and SIGINFO (from ctrl-t)
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);

    #ifdef SIGINFO
    signal(SIGINFO, signal_handler);
    #endif

    std::string abcd_data_address = defaults_abcd_data_address_sub;
    std::string status_address = defaults_tofcalc_status_address;
    std::string data_address = defaults_tofcalc_data_address;
    std::string commands_address = defaults_tofcalc_commands_address;
    std::string config_file;
    unsigned int base_period = defaults_tofcalc_base_period;
    double ns_per_sample = defaults_tofcalc_ns_per_sample;

    int c = 0;
    while ((c = getopt(argc, argv, "hA:S:D:C:T:f:n:vV")) != -1) {
        switch (c) {
            case 'h':
                print_usage(std::string(argv[0]));
                return EXIT_SUCCESS;
            case 'A':
                abcd_data_address = optarg;
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
            case 'T':
                try
                {
                    base_period = std::stoul(optarg);
                }
                catch (std::logic_error &e)
                { }
                break;
            case 'n':
                try
                {
                    ns_per_sample = std::stof(optarg);
                }
                catch (std::logic_error &e)
                { }
                break;
            case 'f':
                config_file = optarg;
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
    global_status.status_address = status_address;
    global_status.data_address = data_address;
    global_status.commands_address = commands_address;
    global_status.config_file = config_file;
    global_status.ns_per_sample = ns_per_sample;

    if (global_status.verbosity > 0) {
        std::cout << "ABCD data socket address: " << abcd_data_address << std::endl;
        std::cout << "Status socket address: " << status_address << std::endl;
        std::cout << "Data socket address: " << data_address << std::endl;
        std::cout << "Commands socket address: " << commands_address << std::endl;
        std::cout << "Verbosity: " << verbosity << std::endl;
        std::cout << "Base period: " << base_period << std::endl;
        std::cout << "Nanoseconds per sample: " << ns_per_sample << std::endl;
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
            current_state = states::CLOSE_SOCKETS;
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
