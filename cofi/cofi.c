/*
 * (C) Copyright 2016, 2022 European Union, Cristiano Lino Fontana
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
    printf("\t-A <address>: Input socket address, default: %s\n", defaults_abcd_data_address_sub);
    printf("\t-D <address>: Output socket address for coincidence data, default: %s\n", defaults_cofi_coincidence_data_address);
    printf("\t-N <address>: Output socket address for anti-coincidence data, default: %s\n", defaults_cofi_anticoincidence_data_address);
    printf("\t-T <period>: Set base period in milliseconds, default: %d\n", defaults_cofi_base_period);
    printf("\t-w <coincidence_window>: Symmetric coincidence window in nanoseconds\n");
    printf("\t-l <left_coincidence_window>: Left coincidence window width in nanoseconds extending to negative times relative to the reference pulse, default: %f\n", defaults_cofi_coincidence_window_left);
    printf("\t-r <right_coincidence_window>: Right coincidence window width in nanoseconds relative to the reference pulse, default: %f\n", defaults_cofi_coincidence_window_right);
    printf("\t-L <left_coincidence_edge>: Left edge of the coincidence window in nanoseconds relative to the reference pulse, default: %f\n", -1 * defaults_cofi_coincidence_window_left);
    printf("\t-m <multiplicity>: Event minimum multiplicity excluding the reference channel, default: %u\n", defaults_cofi_multiplicity);
    printf("\t-n <ns_per_sample>: Nanoseconds per sample, default: %f\n", defaults_cofi_ns_per_sample);
    printf("\t-k: Enable keep reference event, even if there are no coincidences.\n");
    printf("\t-a: Enable the calculation of anticoincidences and the creation of the corresponding socket.\n");

    return;
}

bool in(int channel, int *reference_channels, size_t number_of_channels);
void quicksort(struct event_PSD *events, int64_t low, int64_t high);
int64_t partition(struct event_PSD *events, int64_t low, int64_t high);
void reallocate_array(size_t elements_number, size_t *previous_elements_number, void **elements_array, size_t sizeof_type);

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
    char *output_address_coincidences = defaults_cofi_coincidence_data_address;
    char *output_address_anticoincidences = defaults_cofi_anticoincidence_data_address;
    bool enable_anticoincidences = false;
    float left_coincidence_window_ns = defaults_cofi_coincidence_window_left;
    float right_coincidence_window_ns = defaults_cofi_coincidence_window_right;
    float ns_per_sample = defaults_cofi_ns_per_sample;
    size_t multiplicity = defaults_cofi_multiplicity;

    int c = 0;
    while ((c = getopt(argc, argv, "hA:D:N:S:P:T:w:l:r:L:n:m:kavV")) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'S':
                input_address = optarg;
                break;
            case 'P':
                output_address_coincidences = optarg;
                break;
            case 'A':
                input_address = optarg;
                break;
            case 'D':
                output_address_coincidences = optarg;
                break;
            case 'N':
                output_address_anticoincidences = optarg;
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
            case 'L':
                left_coincidence_window_ns = -1 * atof(optarg);
                break;
            case 'm':
                multiplicity = atoi(optarg);
                break;
            case 'n':
                ns_per_sample = atof(optarg);
                break;
            case 'k':
                keep_reference_event = true;
                break;
            case 'a':
                enable_anticoincidences = true;
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

    const uint64_t left_coincidence_window = (uint64_t)(left_coincidence_window_ns / ns_per_sample);
    const uint64_t right_coincidence_window = (uint64_t)(right_coincidence_window_ns / ns_per_sample);

    if (verbosity > 0) {
        printf("Input socket address: %s\n", input_address);
        printf("Output socket address for coincidences: %s\n", output_address_coincidences);
        printf("Output socket address for anticoincidences: %s\n", output_address_anticoincidences);
        printf("Selected channel(s): ");
        for (int i = 0; i < number_of_references; i++)
        {
            printf("\t%d", reference_channels[i]);
        }
        printf("\n");
        printf("Verbosity: %u\n", verbosity);
        printf("Base period: %u\n", base_period);
        printf("Left coincidence window: %f, (clock steps: %" PRIu64 ")\n", left_coincidence_window_ns, left_coincidence_window);
        printf("Right coincidence window: %f, (clock steps: %" PRIu64 ")\n", right_coincidence_window_ns, right_coincidence_window);
        printf("Nanoseconds per sample: %f\n", ns_per_sample);
        printf("Enable anticoincidences: %s\n", enable_anticoincidences ? "true" : "false");
    }

    // Creates a �MQ context
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

    void *output_socket_coincidences = zmq_socket(context, ZMQ_PUB);
    if (!output_socket_coincidences)
    {
        printf("ERROR: ZeroMQ Error on output socket creation for coincidences\n");
        return EXIT_FAILURE;
    }

    void *output_socket_anticoincidences = NULL;

    if (enable_anticoincidences) {
        output_socket_anticoincidences = zmq_socket(context, ZMQ_PUB);
        if (!output_socket_anticoincidences)
        {
            printf("ERROR: ZeroMQ Error on output socket creation for anticoincidences\n");
            return EXIT_FAILURE;
        }
    }

    const int is = zmq_connect(input_socket, input_address);
    if (is != 0)
    {
        printf("ERROR: ZeroMQ Error on input socket connection: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }

    const int osc = zmq_bind(output_socket_coincidences, output_address_coincidences);
    if (osc != 0)
    {
        printf("ERROR: ZeroMQ Error on output socket binding for coincidences: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }

    if (enable_anticoincidences) {
        const int osa = zmq_bind(output_socket_anticoincidences, output_address_anticoincidences);
        if (osa != 0)
        {
            printf("ERROR: ZeroMQ Error on output socket binding for coincidences: %s\n", zmq_strerror(errno));
            return EXIT_FAILURE;
        }
    }

    // Subscribe to data topic
    zmq_setsockopt(input_socket, ZMQ_SUBSCRIBE, "data_abcd", strlen("data_abcd"));

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

    size_t previous_number_events_coincidence = 0;
    size_t previous_number_events_anticoincidence = 0;
    size_t previous_number_selection_anticoincidence = 0;

    struct event_PSD *events_coincidence = NULL;
    struct event_PSD *events_anticoincidence = NULL;
    bool *selection_anticoincidence = NULL;

    // We start with a random initial guess for the number of events
    if (verbosity > 0)
    {
        printf("Allocating memory for coincidence data.\n");
    }

    reallocate_array(1024,
                     &previous_number_events_coincidence,
                     (void**)&events_coincidence,
                     sizeof(struct event_PSD));

    if (enable_anticoincidences) {
        if (verbosity > 0)
        {
            printf("Allocating memory for anticoincidence data.\n");
        }

        reallocate_array(1024,
                         &previous_number_events_anticoincidence,
                         (void**)&events_anticoincidence,
                         sizeof(struct event_PSD));
        reallocate_array(1024,
                         &previous_number_selection_anticoincidence,
                         (void**)&selection_anticoincidence,
                         sizeof(bool));
    }

    while (terminate_flag == 0)
    {
        char *topic;
        char *input_buffer;
        size_t size;

        const int result = receive_byte_message(input_socket, &topic, (void **)(&input_buffer), &size, true, 0);

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

                size_t index_coincidence_group_start = 0;

                struct event_PSD *events = (void *)input_buffer;

                if ((size % sizeof(struct event_PSD)) != 0)
                {
                    printf("[%zu] ERROR: The buffer size is not a multiple of %zu\n", counter, sizeof(struct event_PSD));
                }
                else
                {
                    reallocate_array(events_number,
                                     &previous_number_events_coincidence,
                                     (void**)&events_coincidence,
                                     sizeof(struct event_PSD));

                    if (enable_anticoincidences) {
                        reallocate_array(events_number,
                                     &previous_number_events_anticoincidence,
                                     (void**)&events_anticoincidence,
                                     sizeof(struct event_PSD));
                        reallocate_array(events_number,
                                     &previous_number_selection_anticoincidence,
                                     (void**)&selection_anticoincidence,
                                     sizeof(bool));

                        for (uint32_t index = 0; index < events_number; index++) {
                            selection_anticoincidence[index] = true;
                        }
                    }

                    if (!events_coincidence
                        || ((!events_anticoincidence || !selection_anticoincidence) && enable_anticoincidences))
                    {
                        printf("[%zu] ERROR: Unable to allocate output buffer(s)\n", counter);
                    }
                    else
                    {
                        quicksort(events, 0, events_number - 1);

                        for (size_t index = 0; index < events_number; index++)
                        {
                            const struct event_PSD this_event = events[index];

                            if (in(this_event.channel, reference_channels, number_of_references))
                            {
                                size_t found_coincidences = 0;

                                // Search backward in time
                                for (int64_t sub_index = (index - 1); sub_index > 0; sub_index--)
                                {
                                    const struct event_PSD that_event = events[sub_index];

                                    if((this_event.timestamp - left_coincidence_window) < that_event.timestamp \
                                       && \
                                       that_event.timestamp < (this_event.timestamp + right_coincidence_window))
                                    {
                                        if (!in(that_event.channel, reference_channels, number_of_references))
                                        {
                                            // Increase already the found_coincidences counter, so
                                            // the first event in the output coincidence group is left
                                            // empty at this stage. It will be filled with the reference
                                            // event afterwards.
                                            found_coincidences += 1;

                                            if (verbosity > 1)
                                            {
                                                printf(" Backward coincidence!!!! i: %zu, j: %" PRId64 ", channel: %" PRIu8 ", timestamps: this: %" PRIu64 ", that: %" PRIu64 ", difference: %" PRId64 "\n", index, sub_index, that_event.channel, this_event.timestamp, that_event.timestamp, ((int64_t)that_event.timestamp) - (int64_t)this_event.timestamp);
                                            }

                                            if ((index_coincidence_group_start + found_coincidences) < defaults_cofi_coincidence_buffer_multiplier * events_number)
                                            {
                                                events_coincidence[index_coincidence_group_start + found_coincidences] = that_event;
                                            }

                                            if (enable_anticoincidences) {
                                                selection_anticoincidence[sub_index] = false;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        break;
                                    }
                                }

                                // Search forward in time
                                for (int64_t sub_index = (index + 1); sub_index < (int64_t)events_number; sub_index++)
                                {
                                    const struct event_PSD that_event = events[sub_index];

                                    if((this_event.timestamp - left_coincidence_window) < that_event.timestamp \
                                       && \
                                       that_event.timestamp < (this_event.timestamp + right_coincidence_window))
                                    {
                                        if (!in(that_event.channel, reference_channels, number_of_references))
                                        {
                                            // See previous comment
                                            found_coincidences += 1;

                                            if (verbosity > 1)
                                            {
                                                printf(" Forward coincidence!!!! i: %zu, j: %" PRId64 ", channel: %" PRIu8 ", timestamps: this: %" PRIu64 ", that: %" PRIu64 ", difference: %" PRId64 "\n", index, sub_index, that_event.channel, this_event.timestamp, that_event.timestamp, ((int64_t)that_event.timestamp) - (int64_t)this_event.timestamp);
                                            }

                                            if ((index_coincidence_group_start + found_coincidences) < defaults_cofi_coincidence_buffer_multiplier * events_number)
                                            {
                                                events_coincidence[index_coincidence_group_start + found_coincidences] = that_event;
                                            }

                                            if (enable_anticoincidences) {
                                                selection_anticoincidence[sub_index] = false;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        break;
                                    }
                                }

                                if (verbosity > 1 && found_coincidences > 0)
                                {
                                    printf(" Event multiplicity: %zu%s\n", found_coincidences, (found_coincidences >= multiplicity) ? " SELECTING GROUP!!!" : "");
                                }

                                if (found_coincidences >= multiplicity) {
                                    quicksort(events_coincidence, index_coincidence_group_start + 1, index_coincidence_group_start + found_coincidences);

                                    if (index_coincidence_group_start < defaults_cofi_coincidence_buffer_multiplier * events_number) {
                                        events_coincidence[index_coincidence_group_start] = this_event;

                                        if (found_coincidences > 0xFF) {
                                            events_coincidence[index_coincidence_group_start].group_counter = 0xFF;
                                        } else {
                                            events_coincidence[index_coincidence_group_start].group_counter = (uint8_t)found_coincidences;
                                        }

                                        index_coincidence_group_start += (1 + found_coincidences);
                                    }

                                    if (enable_anticoincidences) {
                                        selection_anticoincidence[index] = false;
                                    }
                                } else if (keep_reference_event) {
                                    if (index_coincidence_group_start < defaults_cofi_coincidence_buffer_multiplier * events_number) {
                                        events_coincidence[index_coincidence_group_start] = this_event;
                                        events_coincidence[index_coincidence_group_start].group_counter = 0;
                                        index_coincidence_group_start += 1;
                                    }

                                    if (enable_anticoincidences) {
                                        selection_anticoincidence[index] = false;
                                    }
                                }
                            }
                        }

                        const size_t output_size = index_coincidence_group_start * sizeof(struct event_PSD);

                        // Compute the new topic
                        char new_topic[defaults_all_topic_buffer_size];
                        snprintf(new_topic, defaults_all_topic_buffer_size, "data_abcd_events_v0_s%zu", output_size);

                        send_byte_message(output_socket_coincidences, new_topic, (void *)events_coincidence, output_size, 0);
                        msg_ID += 1;

                        if (verbosity > 0)
                        {
                            printf("Sending message with coincidences with topic: %s\n", new_topic);
                        }

                        if (enable_anticoincidences) {
                            size_t found_anticoincidences = 0;

                            for (size_t index = 0; index < events_number; index++)
                            {
                                const struct event_PSD this_event = events[index];

                                if (selection_anticoincidence[index] == true) {
                                    // This should be memory safe, as the
                                    // selection is a sub-set of the incoming
                                    // events.
                                    events_anticoincidence[found_anticoincidences] = this_event;

                                    if (verbosity > 1) {
                                        printf(" anticoincidence: i: %zu, found: %zu; channel: %" PRIu8 ", timestamp: %" PRIu64 "\n", \
                                               index, found_anticoincidences, this_event.channel, this_event.timestamp);
                                    }

                                    found_anticoincidences += 1;
                                }
                            }

                            if (verbosity > 0)
                            {
                                printf("events_number: %zu; coincidences_number: %zu; anticoincidences_number: %zu;\n", \
                                       events_number, index_coincidence_group_start, found_anticoincidences);
                            }

                            if (found_anticoincidences > 0) {
                                const size_t output_size = found_anticoincidences * sizeof(struct event_PSD);

                                // Compute the new topic
                                char new_topic[defaults_all_topic_buffer_size];
                                snprintf(new_topic, defaults_all_topic_buffer_size, "data_abcd_events_v0_s%zu", output_size);

                                send_byte_message(output_socket_anticoincidences, new_topic, (void *)events_anticoincidence, output_size, 0);
                                msg_ID += 1;

                                if (verbosity > 0)
                                {
                                    printf("Sending message with anticoincidences with topic: %s\n", new_topic);
                                }
                            }
                        }
                    }
                }

                const clock_t event_stop = clock();

                if (verbosity > 0)
                {
                    const float elaboration_time = (float)(event_stop - event_start) / CLOCKS_PER_SEC * 1000;
                    const float elaboration_speed_MB = size / elaboration_time * 1000.0 / 1024.0 / 1024.0;
                    const float elaboration_speed_events = events_number / elaboration_time;

                    printf("size: %zu; events_number: %zu; coincidences_number: %zu; ratio: %.2f%%; elaboration_time: %f ms; elaboration_speed: %.1f MBi/s, %.1f kevts/s\n", \
                           size, events_number, index_coincidence_group_start, index_coincidence_group_start / (float)events_number * 100, elaboration_time, elaboration_speed_MB, elaboration_speed_events);
                }
            }
            else if (strstr(topic, "data_abcd_waveforms_v0") == topic)
            {
                if (verbosity > 0) {
                    printf("Forwarding waveforms message\n");
                }

                send_byte_message(output_socket_coincidences, topic, (void *)input_buffer, size, 0);
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

        if (verbosity > 2)
        {
            printf("counter: %zu; msg_counter: %zu, msg_ID: %zu\n", counter, msg_counter, msg_ID);
        }
    }

    free(reference_channels);
    free(events_coincidence);

    if (enable_anticoincidences) {
        free(events_anticoincidence);
        free(selection_anticoincidence);
    }

    // Wait a bit to allow the sockets to deliver
    nanosleep(&slow_joiner_wait, NULL);
    //usleep(defaults_all_slow_joiner_wait * 1000);

    const int ic = zmq_close(input_socket);
    if (ic != 0)
    {
        printf("ERROR: ZeroMQ Error on input socket close: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }

    const int occ = zmq_close(output_socket_coincidences);
    if (occ != 0)
    {
        printf("ERROR: ZeroMQ Error on output socket for coincidences close: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }

    if (enable_anticoincidences) {
        const int oac = zmq_close(output_socket_anticoincidences);
        if (oac != 0)
        {
            printf("ERROR: ZeroMQ Error on output socket for anticoincidences close: %s\n", zmq_strerror(errno));
            return EXIT_FAILURE;
        }
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

void quicksort(struct event_PSD *events, int64_t low, int64_t high)
{
    if (low < high)
    {
        const int64_t pivot_index = partition(events, low, high);

        //printf("Quicksort: low: %lu, high: %lu, pivot: %lu\n", low, high, pivot_index);

        quicksort(events, low, pivot_index - 1);
        quicksort(events, pivot_index + 1, high);
    }
}

int64_t partition(struct event_PSD *events, int64_t low, int64_t high)
{
    const struct event_PSD pivot = events[high];

    int64_t i = low - 1;

    for (int64_t j = low; j <= high - 1; j++)
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

void reallocate_array(size_t elements_number, size_t *previous_elements_number, void **elements_array, size_t sizeof_type)
{
    if (elements_number > (*previous_elements_number)) {
        *previous_elements_number = elements_number;

        // At most we can have a elements_number**2 number of coincidences,
        // but that would be too much for a buffer...
        // We can estimate a very wide coincidence window and say that there
        // will be at most defaults_cofi_coincidence_buffer_multiplier * elements_number coincidences.
        void *new_elements_array = realloc((*elements_array),
                                           elements_number * sizeof_type * defaults_cofi_coincidence_buffer_multiplier);

        if (!new_elements_array) {
            printf("ERROR: reallocate_array(): Unable to allocate elements_array memory\n");

            (*elements_array) = NULL;
        } else {
            (*elements_array) = new_elements_array;
        }
    }
}
