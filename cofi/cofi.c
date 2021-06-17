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
// This macro is to enable snprintf() in macOS
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
#include "events.h"
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
    printf("Usage: %s [options] <reference_channels>\n", name);
    printf("\n");
    printf("Datastream filter that selects the events that are in coincidence with a set of channels in a defined time window.\n");
    printf("\n");
    printf("Optional arguments:\n");
    printf("\t-h: Display this message\n");
    printf("\t-v: Set verbose execution\n");
    printf("\t-V: Set verbose execution with more output\n");
    printf("\t-S <address>: SUB socket address, default: %s\n", defaults_abcd_data_address_sub);
    printf("\t-P <address>: PUB socket address, default: %s\n", defaults_cofi_data_address);
    printf("\t-T <period>: Set base period in milliseconds, default: %d\n", defaults_cofi_base_period);
    printf("\t-w <coincidence_window>: Symmetric coincidence window in nanoseconds\n");
    printf("\t-l <left_coincidence_window>: Left edge of coincidence window in nanoseconds, default: %f\n", defaults_cofi_coincidence_window_left);
    printf("\t-r <right_coincidence_window>: Right edge of coincidence window in nanoseconds, default: %f\n", defaults_cofi_coincidence_window_right);
    printf("\t-n <ns_per_sample>: Nanoseconds per sample, default: %f\n", defaults_cofi_ns_per_sample);
    printf("\t-k: Enable keep reference event, even if there are no coincidences.\n");

    return;
}

bool in(int channel, int *reference_channels, size_t number_of_channels);
void quicksort(struct event_PSD *events, intmax_t low, intmax_t high);
intmax_t partition(struct event_PSD *events, intmax_t low, intmax_t high);

int main(int argc, char *argv[])
{
    // Register the handler for SIGTERM (from kill), SIGINT (from ctrl-c)
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);

    bool keep_reference_event = false;
    unsigned int verbosity = 0;
    unsigned int base_period = defaults_cofi_base_period;
    char *input_address = defaults_abcd_data_address_sub;
    char *output_address = defaults_cofi_data_address;
    float left_coincidence_window_ns = defaults_cofi_coincidence_window_left;
    float right_coincidence_window_ns = defaults_cofi_coincidence_window_right;
    float ns_per_sample = defaults_cofi_ns_per_sample;

    int c = 0;
    while ((c = getopt(argc, argv, "hS:P:T:w:l:r:n:kvV")) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'S':
                input_address = optarg;
                break;
            case 'P':
                output_address = optarg;
                break;
            case 'T':
                base_period = atoi(optarg);
                break;
            case 'w':
                left_coincidence_window_ns = atof(optarg);
                right_coincidence_window_ns = atof(optarg);
                break;
            case 'l':
                left_coincidence_window_ns = atof(optarg);
                break;
            case 'r':
                right_coincidence_window_ns = atof(optarg);
                break;
            case 'n':
                ns_per_sample = atof(optarg);
                break;
            case 'k':
                keep_reference_event = true;
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

    const int number_of_references = argc - optind;

    if (number_of_references <= 0)
    {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    int *reference_channels = (int *)malloc(number_of_references * sizeof(int));

    for (int i = 0; i < number_of_references; i++)
    {
        reference_channels[i] = atoi(argv[optind + i]);
    }

    const uintmax_t left_coincidence_window = (uintmax_t)(left_coincidence_window_ns / ns_per_sample);
    const uintmax_t right_coincidence_window = (uintmax_t)(right_coincidence_window_ns / ns_per_sample);

    if (verbosity > 0) {
        printf("Input socket address: %s\n", input_address);
        printf("Output socket address: %s\n", output_address);
        printf("Selected channel(s): ");
        for (int i = 0; i < number_of_references; i++)
        {
            printf("\t%d", reference_channels[i]);
        }
        printf("\n");
        printf("Verbosity: %u\n", verbosity);
        printf("Base period: %u\n", base_period);
        printf("Left coincidence window: %f, (clock steps: %" PRIuMAX ")\n", left_coincidence_window_ns, left_coincidence_window);
        printf("Right coincidence window: %f, (clock steps: %" PRIuMAX ")\n", right_coincidence_window_ns, right_coincidence_window);
        printf("Nanoseconds per sample: %f\n", ns_per_sample);
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
    zmq_setsockopt(input_socket, ZMQ_SUBSCRIBE, defaults_abcd_data_events_topic, strlen(defaults_abcd_data_events_topic));

    // Wait a bit to prevent the slow-joiner syndrome
    struct timespec slow_joiner_wait;
    slow_joiner_wait.tv_sec = defaults_all_slow_joiner_wait / 1000;
    slow_joiner_wait.tv_nsec = (defaults_all_slow_joiner_wait % 1000) * 1000000L;
    nanosleep(&slow_joiner_wait, NULL);
    //usleep(defaults_all_slow_joiner_wait * 1000);

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
        char *input_buffer;
        size_t size;

        const int result = receive_byte_message(input_socket, &topic, (void **)(&input_buffer), &size, true, verbosity);

        if (result == EXIT_FAILURE)
        {
            printf("[%zu] ERROR: Some error occurred!!!\n", counter);
        }
        else if (size == 0 && result == EXIT_SUCCESS)
        {
            if (verbosity > 2)
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
            if (strstr(topic, "data_abcd_events_v0") == topic)
            {
                const clock_t event_start = clock();

                const size_t events_number = size / sizeof(struct event_PSD);

                size_t coincidences_number = 0;

                struct event_PSD *events = (void *)input_buffer;

                if ((size % sizeof(struct event_PSD)) != 0)
                {
                    printf("[%zu] ERROR: The buffer size is not a multiple of %zu\n", counter, sizeof(struct event_PSD));
                }
                else
                {
                    // At most we can have a events_number**2 number of coincidences,
                    // but that would be too much for a buffer...
                    // We can estimate a very wide coincidence window and say that there
                    // will be at most defaults_cofi_coincidence_buffer_multiplier * events_number coincidences.
                    struct event_PSD *coincidence_events = malloc(sizeof(struct event_PSD) * events_number * defaults_cofi_coincidence_buffer_multiplier);

                    if (coincidence_events == NULL)
                    {
                        printf("[%zu] ERROR: Unable to allocate output buffer\n", counter);
                    }
                    else
                    {
                        quicksort(events, 0, events_number - 1);

                        //uint64_t prev_timestamp = 0;

                        for (size_t i = 0; i < events_number; i++)
                        {
                            const struct event_PSD this_event = events[i];
                            //printf("i: %zu, ch: %d, t: %lu\n", i, this_event.channel, this_event.timestamp);

                            //if (prev_timestamp > this_event.timestamp)
                            //{
                            //    printf("ERROR: Not ordered!!! i: %zu, timestamp: %lu, prev_timestamp: %lu\n", i, this_event.timestamp, prev_timestamp);
                            //}
                            //prev_timestamp = this_event.timestamp;

                            if (in(this_event.channel, reference_channels, number_of_references))
                            {
                                uintmax_t found_coincidences = 0;

                                for (intmax_t j = (i + 1); j < (intmax_t)events_number; j++)
                                {
                                    const struct event_PSD that_event = events[j];

                                    if((this_event.timestamp - left_coincidence_window) < that_event.timestamp \
                                       && \
                                       that_event.timestamp < (this_event.timestamp + right_coincidence_window))
                                    {
                                        if (!in(that_event.channel, reference_channels, number_of_references))
                                        {
                                            found_coincidences += 1;

                                            if (verbosity > 1)
                                            {
                                                printf(" Forward coincidence!!!! i: %zu, j: %" PRIdMAX ", channel: %" PRIu8 ", timestamps: this: %" PRIu64 ", that: %" PRIu64 ", difference: %" PRIdMAX "\n", i, j, that_event.channel, this_event.timestamp, that_event.timestamp, ((intmax_t)that_event.timestamp) - (intmax_t)this_event.timestamp);
                                            }

                                            if (coincidences_number < defaults_cofi_coincidence_buffer_multiplier * events_number)
                                            {
                                                coincidence_events[coincidences_number] = that_event;
                                                coincidences_number += 1;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        break;
                                    }
                                }

                                for (intmax_t j = (i - 1); j > 0; j--)
                                {
                                    const struct event_PSD that_event = events[j];

                                    if((this_event.timestamp - left_coincidence_window) < that_event.timestamp \
                                       && \
                                       that_event.timestamp < (this_event.timestamp + right_coincidence_window))
                                    {
                                        if (!in(that_event.channel, reference_channels, number_of_references))
                                        {
                                            found_coincidences += 1;

                                            if (verbosity > 1)
                                            {
                                                printf(" Backward coincidence!!!! i: %zu, j: %" PRIdMAX ", channel: %" PRIu8 ", timestamps: this: %" PRIu64 ", that: %" PRIu64 ", difference: %" PRIdMAX "\n", i, j, that_event.channel, this_event.timestamp, that_event.timestamp, ((intmax_t)that_event.timestamp) - (intmax_t)this_event.timestamp);
                                            }

                                            if (coincidences_number < defaults_cofi_coincidence_buffer_multiplier * events_number)
                                            {
                                                coincidence_events[coincidences_number] = that_event;
                                                coincidences_number += 1;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        break;
                                    }
                                }

                                if (((found_coincidences > 0) || keep_reference_event)
                                    && (coincidences_number < defaults_cofi_coincidence_buffer_multiplier * events_number))
                                {
                                    coincidence_events[coincidences_number] = this_event;
                                    coincidences_number += 1;
                                }
                            }
                        }

                        const size_t output_size = coincidences_number * sizeof(struct event_PSD);

                        // Compute the new topic
                        char new_topic[defaults_all_topic_buffer_size];
                        // I am not sure if snprintf is standard or not, apprently it is in the C99 standard
                        snprintf(new_topic, defaults_all_topic_buffer_size, "data_abcd_events_v0_s%zu", output_size);

                        send_byte_message(output_socket, new_topic, (void *)coincidence_events, output_size, verbosity);
                        msg_ID += 1;

                        if (verbosity > 0)
                        {
                            printf("Sending message with topic: %s\n", new_topic);
                        }

                        free(coincidence_events);
                    }
                }

                const clock_t event_stop = clock();

                if (verbosity > 0)
                {
                    const float elaboration_time = (float)(event_stop - event_start) / CLOCKS_PER_SEC * 1000;
                    const float elaboration_speed = size / elaboration_time * 1000.0 / 1024.0 / 1024.0;

                    printf("size: %zu; events_number: %zu; coincidences_number: %zu; ratio: %.2f%%; elaboration_time: %f ms; elaboration_speed: %f MBi/s\n", size, events_number, coincidences_number, coincidences_number / (float)events_number * 100, elaboration_time, elaboration_speed);
                }
            }

            msg_counter += 1;

            // Remember to free buffers
            free(topic);
            free(input_buffer);
        }
        else
        {
            printf("[%zu] ERROR: What?!?!?!\n", counter);
        }

        counter += 1;

        // Putting a delay in order not to fill-up the queues
        nanosleep(&wait, NULL);
        //usleep(base_period * 1000);

        if (verbosity > 2)
        {
            printf("counter: %zu; msg_counter: %zu, msg_ID: %zu\n", counter, msg_counter, msg_ID);
        }
    }

    free(reference_channels);

    // Wait a bit to allow the sockets to deliver
    nanosleep(&slow_joiner_wait, NULL);
    //usleep(defaults_all_slow_joiner_wait * 1000);

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

bool in(int channel, int *reference_channels, size_t number_of_channels)
{
    for(size_t i = 0; i < number_of_channels; i++)
    {
        if (channel == reference_channels[i])
        {
            return true;
        }
    }

    return false;
}

void quicksort(struct event_PSD *events, intmax_t low, intmax_t high)
{
    if (low < high)
    {
        const intmax_t pivot_index = partition(events, low, high);

        //printf("Quicksort: low: %lu, high: %lu, pivot: %lu\n", low, high, pivot_index);

        quicksort(events, low, pivot_index - 1);
        quicksort(events, pivot_index + 1, high);
    }
}

intmax_t partition(struct event_PSD *events, intmax_t low, intmax_t high)
{
    const struct event_PSD pivot = events[high];

    intmax_t i = low - 1;

    for (intmax_t j = low; j <= high - 1; j++)
    {
        if (events[j].timestamp <= pivot.timestamp)
        {
            i += 1;

            const struct event_PSD temp = events[j];
            events[j] = events[i];
            events[i] = temp;
        }
    }

    const struct event_PSD temp = events[high];
    events[high] = events[i + 1];
    events[i + 1] = temp;
    
    return i + 1;
}
