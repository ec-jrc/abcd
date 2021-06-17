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

unsigned int verbosity = defaults_fifo_verbosity;
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
        std::cout << '[' << time_buffer << "] " << "Running" << std::endl;
    }
    #endif
}

void print_usage(const std::string &name = std::string("fifo")) {
    char data_address[sizeof("[wxyz://255.255.255.255:65535]")];
    address_bind_to_connect(data_address, sizeof(data_address), defaults_abcd_data_address, defaults_abcd_ip);


    std::cout << "Usage: " << name << " [options]" << std::endl;
    std::cout << "\t-h: Display this message" << std::endl;
    std::cout << "\t-A <address>: ABCD data socket address, default: " << data_address << std::endl;
    std::cout << "\t-S <address>: Status socket address, default: ";
    std::cout << defaults_fifo_status_address << std::endl;
    std::cout << "\t-R <address>: Reply socket address, default: ";
    std::cout << defaults_fifo_reply_address << std::endl;
    std::cout << "\t-T <period>: Set base period in milliseconds, default: ";
    std::cout << defaults_fifo_base_period << std::endl;
    std::cout << "\t-t <subscription_topic>: Set the topic to subscribe to, default: ";
    std::cout << defaults_abcd_data_events_topic << std::endl;
    std::cout << "\t-v: Set verbose execution" << std::endl;
    std::cout << "\t-V: Set more verbose execution" << std::endl;
    std::cout << "\t-E <expiration_time>: Set base expiration time in seconds, default: ";
    std::cout << defaults_fifo_expiration_time / 1000000000ULL << std::endl;

    return;
}

int main(int argc, char *argv[])
{
    // Show "splash screen"
    std::cout << std::endl;
    std::cout << "========================" << std::endl;
    std::cout << " FIFO software - v. 0.1 " << std::endl;
    std::cout << "========================" << std::endl;
    std::cout << std::endl;

    // Register the handler for SIGTERM (from kill), SIGINT (from ctrl-c) and SIGINFO (from ctrl-t)
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);

    #ifdef SIGINFO
    signal(SIGINFO, signal_handler);
    #endif

    std::string abcd_data_address = defaults_abcd_data_address_sub;
    std::string status_address = defaults_fifo_status_address;
    std::string reply_address = defaults_fifo_reply_address;
    std::string subscription_topic = defaults_abcd_data_events_topic;
    unsigned int base_period = defaults_fifo_base_period;
    binary_fifo::long_int expiration_time = defaults_fifo_expiration_time;

    int c = 0;
    while ((c = getopt(argc, argv, "hA:S:R:t:T:vVE:")) != -1) {
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
            case 'R':
                reply_address = optarg;
                break;
            case 't':
                subscription_topic = optarg;
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
            case 'E':
                try
                {
                    expiration_time = std::stoul(optarg) * 1000000000ULL;
                }
                catch (std::logic_error &e)
                { }
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
    global_status.reply_address = reply_address;
    global_status.subscription_topic = subscription_topic;
    global_status.expiration_time = expiration_time;
    global_status.fifo.set_expiration_time(global_status.expiration_time);

    if (global_status.verbosity > 0) {
        std::cout << "ABCD data socket address: " << abcd_data_address << std::endl;
        std::cout << "Status socket address: " << status_address << std::endl;
        std::cout << "Reply socket address: " << reply_address << std::endl;
        std::cout << "Subscription topic: " << subscription_topic << std::endl;
        std::cout << "Verbosity: " << verbosity << std::endl;
        std::cout << "Base period: " << base_period << std::endl;
        std::cout << "Expiration time: " << expiration_time / 1000000000ULL << " s" << std::endl;
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

            std::cout << '[' << time_buffer << "] Signal: ";
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
