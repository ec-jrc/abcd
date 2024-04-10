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

// This macro is to use nanosleep even with compilation flag: -std=c99
#define _POSIX_C_SOURCE 199309L
#define _C99_SOURCE

// For nanosleep()
#include <time.h>
// Fot getopt
#include <getopt.h>
// For malloc
#include <stdlib.h>
// For memcpy
#include <string.h>
// For ints with fixed size
#include <stdint.h>
#include <inttypes.h>
// For kernel signals management
#include <signal.h>
// For snprintf()
#include <stdio.h>
// For boolean datatype
#include <stdbool.h>
// For the time functions for ISO time
#include <time.h>

#include <zmq.h>

#include "defaults.h"
#include "socket_functions.h"

unsigned int terminate_flag = 0;

// Handle standard signals
// SIGTERM (from kill): terminates kindly forcing the status to the closing branch of the state machine.
// SIGINT (from ctrl-c): same behaviour as SIGTERM
// SIGHUP (from shell processes): same behaviour as SIGTERM

void signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM || signum == SIGHUP)
    {
        terminate_flag = 1;
    }
}

void print_usage(const char *name) {
    printf("Usage: %s [options]\n", name);
    printf("\n");
    printf("Example program that reads from a PUB socket.\n");
    printf("\n");
    printf("Optional arguments:\n");
    printf("\t-h: Display this message\n");
    printf("\t-v: Set verbose execution\n");
    printf("\t-d: Enable buffer display as a string printed on stdout\n");
    printf("\t-S <address>: SUB socket address, default: %s\n", defaults_abcd_status_address_sub);
    printf("\t-t <topic>: Subscription topic, default: %s\n", defaults_abcd_status_topic);
    printf("\t-T <period>: Set base period in milliseconds, default: %d\n", defaults_wafi_base_period);

    return;
}

int main(int argc, char *argv[])
{
    // Register the handler for SIGTERM (from kill), SIGINT (from ctrl-c)
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);

    unsigned int verbosity = 0;
    unsigned int base_period = defaults_wafi_base_period;
    char *input_address = defaults_abcd_status_address_sub;
    char *subscription_topic = defaults_abcd_status_topic;
    bool display_buffer = false;

    int c = 0;
    while ((c = getopt(argc, argv, "hdS:t:T:v")) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'd':
                display_buffer = true;
                break;
            case 'S':
                input_address = optarg;
                break;
            case 'T':
                base_period = atoi(optarg);
                break;
            case 't':
                subscription_topic = optarg;
                break;
            case 'v':
                verbosity = 1;
                break;
            default:
                printf("Unknown command: %c", c);
                break;
        }
    }

    if (verbosity > 0) {
        printf("Input socket address: %s\n", input_address);
        printf("Subscription topic: %s\n", subscription_topic);
        printf("Verbosity: %u\n", verbosity);
        printf("Base period: %u\n", base_period);
    }

    // Creates a ØMQ context
    void *context = zmq_ctx_new();
    if (!context)
    {
        printf("ERROR: ZeroMQ Error on context creation");
        return EXIT_FAILURE;
    }

    void *input_socket = zmq_socket(context, ZMQ_SUB);
    if (!input_socket)
    {
        printf("ERROR: ZeroMQ Error on input socket creation\n");
        return EXIT_FAILURE;
    }

    const int is = zmq_connect(input_socket, input_address);
    if (is != 0)
    {
        printf("ERROR: ZeroMQ Error on input socket binding: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }

    // Subscribe to data topic
    zmq_setsockopt(input_socket, ZMQ_SUBSCRIBE, subscription_topic, strlen(subscription_topic));

    // Wait a bit to prevent the slow-joiner syndrome
    struct timespec slow_joiner_wait;
    slow_joiner_wait.tv_sec = defaults_all_slow_joiner_wait / 1000;
    slow_joiner_wait.tv_nsec = (defaults_all_slow_joiner_wait % 1000) * 1000000L;
    nanosleep(&slow_joiner_wait, NULL);
    //usleep(defaults_abcd_zmq_delay * 1000);

    // Setting up the wait for the main loop
    // If you're asking why, on earth, are we using something so complicated,
    // on the man page of usleep it is said:
    // 4.3BSD, POSIX.1-2001.  POSIX.1-2001  declares  this  function  obsolete;  use  nanosleep(2)
    struct timespec wait;
    wait.tv_sec = base_period / 1000;
    wait.tv_nsec = (base_period % 1000) * 1000000L;

    size_t counter = 0;
    size_t msg_counter = 0;
    size_t msg_ID = 0;

    while (terminate_flag == 0)
    {
        char *topic;
        char *buffer;
        size_t size;

        const int result = receive_byte_message(input_socket, &topic, (void **)(&buffer), &size, true, verbosity);

        if (result == EXIT_FAILURE)
        {
            printf("[%zu] ERROR: Some error occurred!!!\n", counter);
        }
        else if (size == 0 && result == EXIT_SUCCESS)
        {
            if (verbosity > 1)
            {
                printf("[%zu] No message available\n", counter);
            }
        }
        else if (size > 0 && result == EXIT_SUCCESS)
        {
            if (verbosity > 0)
            {
                printf("[%zu] Message received!!! (topic: %s)\n", counter, topic);
            }

            printf("[%zu] Topic (len: %zu): %s\n", msg_counter, strlen(topic), topic);
            printf("[%zu] Buffer length: %zu\n", msg_counter, strlen((char*)buffer));
            
            if (display_buffer)
            {
                // Copying the buffer to a C-style string
                char *message_string = calloc(size + 1, 1);
                memcpy(message_string, buffer, size);

                printf("[%zu] Buffer: %s\n", msg_counter, message_string);

                free(message_string);
            }

            msg_counter += 1;

            // Remember to free buffers
            free(topic);
            free(buffer);
        }
        else
        {
            printf("[%zu] ERROR: What?!?!?!\n", counter);
        }

        counter += 1;

        // Putting a delay in order not to fill-up the queues
        nanosleep(&wait, NULL);
        //usleep(base_period * 1000);

        if (verbosity > 1)
        {
            printf("counter: %zu; msg_counter: %zu, msg_ID: %zu\n", counter, msg_counter, msg_ID);
        }
    }

    // Wait a bit to allow the sockets to deliver
    nanosleep(&slow_joiner_wait, NULL);
    //usleep(defaults_abcd_zmq_delay * 1000);

    const int ic = zmq_close(input_socket);
    if (ic != 0)
    {
        printf("ERROR: ZeroMQ Error on input socket close: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }
    
    const int cc = zmq_ctx_destroy(context);
    if (cc != 0)
    {
        printf("ERROR: ZeroMQ Error on context destroy: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
