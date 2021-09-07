/*
 * (C) Copyright 2016,2020 Cristiano Lino Fontana
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

#include <jansson.h>
#include <zmq.h>

#include "defaults.h"
#include "socket_functions.h"
#include "jansson_socket_functions.h"

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
    printf("Selects only one waveform per channel per message and converts it to JSON, used only for the GUI.\n");
    printf("\n");
    printf("Optional arguments:\n");
    printf("\t-h: Display this message\n");
    printf("\t-v: Set verbose execution\n");
    printf("\t-v: Set more verbose execution\n");
    //printf("\t-N <number>: Number of waveforms to keep per channel, default: %s\n", defaults_wafi_number);
    printf("\t-A <address>: ABCD data socket address, default: %s\n", defaults_abcd_data_address_sub);
    printf("\t-D <address>: Data output socket address, default: %s\n", defaults_wafi_data_address);
    printf("\t-T <period>: Set base period in milliseconds, default: %d\n", defaults_wafi_base_period);

    return;
}

bool in_array(json_int_t channel, json_t *array)
{
    if (json_is_array(array))
    {
        size_t index = 0;
        json_t *value = NULL;

        json_array_foreach(array, index, value) {
            // This block of code uses the index and value variables
            // that are passed to the json_array_foreach() macro.

            if (json_is_integer(value))
            {
                const json_int_t this_channel = json_integer_value(value);
        
                if (this_channel == channel)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

int main(int argc, char *argv[])
{
    // Register the handler for SIGTERM (from kill), SIGINT (from ctrl-c)
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);

    unsigned int verbosity = 0;
    unsigned int base_period = defaults_wafi_base_period;
    char *input_address = defaults_abcd_data_address_sub;
    char *output_address = defaults_wafi_data_address;

    int c = 0;
    while ((c = getopt(argc, argv, "hA:D:T:vV")) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'A':
                input_address = optarg;
                break;
            case 'D':
                output_address = optarg;
                break;
            case 'T':
                base_period = atoi(optarg);
                break;
            case 'v':
                verbosity = 1;
                break;
            case 'V':
                verbosity = 2;
                break;
            default:
                printf("Unknown command: %c", c);
                break;
        }
    }

    if (verbosity > 0) {
        printf("Input socket address: %s\n", input_address);
        printf("Output socket address: %s\n", output_address);
        printf("Verbosity: %u\n", verbosity);
        printf("Base period: %u\n", base_period);
    }

    // Creates a ZeroMQ context
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

    void *output_socket = zmq_socket(context, ZMQ_PUB);
    if (!output_socket)
    {
        printf("ERROR: ZeroMQ Error on output socket creation\n");
        return EXIT_FAILURE;
    }

    const int is = zmq_connect(input_socket, input_address);
    if (is != 0)
    {
        printf("ERROR: ZeroMQ Error on input socket binding: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }

    const int os = zmq_bind(output_socket, output_address);
    if (os != 0)
    {
        printf("ERROR: ZeroMQ Error on output socket connection: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }

    // Subscribe to data topic
    zmq_setsockopt(input_socket, ZMQ_SUBSCRIBE, defaults_abcd_data_waveforms_topic, strlen(defaults_abcd_data_waveforms_topic));

    // Wait a bit to prevent the slow-joiner syndrome
    struct timespec slow_joiner_wait;
    slow_joiner_wait.tv_sec = defaults_all_slow_joiner_wait / 1000;
    slow_joiner_wait.tv_nsec = (defaults_all_slow_joiner_wait % 1000) * 1000000L;
    nanosleep(&slow_joiner_wait, NULL);

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

        const int result = receive_byte_message(input_socket, &topic, (void **)(&buffer), &size, true, 0);

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

            // Check if the topic is the right one
            if (strstr(topic, "data_abcd_waveforms_v0") == topic)
            {
                size_t all_events = 0;
                size_t selected_events = 0;
                size_t input_offset = 0;

                json_t *active_channels = json_array();
                json_t *channels = json_array();

                while (input_offset < size)
                {
                    const uint64_t timestamp = *((uint64_t*)(buffer + input_offset));
                    const uint8_t this_channel = *((uint8_t*)(buffer + input_offset + 8));
                    const uint32_t samples_number = *((uint32_t*)(buffer + input_offset + 9));
                    const uint8_t gates_number = *((uint8_t*)(buffer + input_offset + 13));
    
                    // Compute the waveform event size
                    const size_t this_size = 14 + samples_number * sizeof(uint16_t) + gates_number * samples_number * sizeof(uint8_t);
    
                    if (!in_array(this_channel, active_channels))
                    {
                        json_array_append_new(active_channels, json_integer(this_channel));

                        json_t *this_samples = json_array();
                        json_t *these_gates = json_array();

                        const size_t samples_offset = input_offset + 14;

                        for (size_t i = 0; i < samples_number; i++)
                        {
                            const size_t this_offset = samples_offset + i * sizeof(uint16_t);
                            const uint16_t sample = *((uint16_t*)(buffer + this_offset));
                            json_array_append_new(this_samples, json_integer(sample));
                        }

                        const size_t gates_offset = samples_offset + samples_number * sizeof(uint16_t);

                        for (size_t j = 0; j < gates_number; j++)
                        {
                            json_t *gates_samples = json_array();

                            for (size_t i = 0; i < samples_number; i++)
                            {
                                const size_t this_offset = gates_offset + j * samples_number * sizeof(uint8_t) + i * sizeof(uint8_t);
                                const uint8_t sample = *((uint8_t*)(buffer + this_offset));
                                json_array_append_new(gates_samples, json_integer(sample));
                            }

                            json_array_append_new(these_gates, gates_samples);
                        }

                        json_t *json_channel = json_object();

                        json_object_set_new_nocheck(json_channel, "id", json_integer(this_channel));
                        json_object_set_new_nocheck(json_channel, "timestamp", json_integer(timestamp));
                        json_object_set_new_nocheck(json_channel, "samples", this_samples);
                        json_object_set_new_nocheck(json_channel, "gates", these_gates);

                        json_array_append_new(channels, json_channel);

                        selected_events += 1;
                    }

                    if (verbosity > 1)
                    {
                        printf("size: %zu; this_channel: %" PRIu8 "; samples_number: %" PRIu32 "; gates_number: %" PRIu8 ", this_size: %zu; input_offset: %zu\n", size, this_channel, samples_number, gates_number, this_size, input_offset);
                    }
    
                    all_events += 1;
                    input_offset += this_size;
                }

                time_t raw_time;
                time(&raw_time);

                const struct tm *time_info = localtime(&raw_time);
                char time_buffer[sizeof("[2011-10-08T07:07:09Z+0100]")];
                strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%dT%H:%M:%S%z", time_info);

                if (verbosity > 0)
                {
                    printf("Time buffer: %s\n", time_buffer);
                }

                json_t *json_message = json_object();

                json_object_set_new_nocheck(json_message, "module", json_string("wadi"));
                json_object_set_new_nocheck(json_message, "timestamp", json_string(time_buffer));
                json_object_set_new_nocheck(json_message, "msg_ID", json_integer(msg_ID));
                json_object_set_new_nocheck(json_message, "active_channels", active_channels);
                json_object_set_new_nocheck(json_message, "channels", channels);

                send_json_message(output_socket, defaults_wadi_data_waveforms_topic, json_message, 0);
                msg_ID += 1;

                json_decref(json_message);

                if (verbosity > 0)
                {
                    printf("size: %zu; all_events: %zu; selected_events: %zu; input_offset: %zu\n", size, all_events, selected_events, input_offset);
                }
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

        if (verbosity > 1)
        {
            printf("counter: %zu; msg_counter: %zu, msg_ID: %zu\n", counter, msg_counter, msg_ID);
        }
    }

    // Wait a bit to allow the sockets to deliver
    nanosleep(&slow_joiner_wait, NULL);

    const int ic = zmq_close(input_socket);
    if (ic != 0)
    {
        printf("ERROR: ZeroMQ Error on input socket close: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }
    
    const int oc = zmq_close(output_socket);
    if (oc != 0)
    {
        printf("ERROR: ZeroMQ Error on output socket close: %s\n", zmq_strerror(errno));
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
