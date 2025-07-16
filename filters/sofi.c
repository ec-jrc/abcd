/*
 * (C) Copyright 2016, 2022, 2024 European Union, Cristiano Lino Fontana
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
// For qsort
#include <stdlib.h>
// For the time functions for ISO time
#include <time.h>

#include <zmq.h>

#include "defaults.h"
#include "events.h"
#include "socket_functions.h"

#define INITIAL_BUFFER_SIZE 1024

struct event_pointer
{
    size_t index;
    uint64_t timestamp;
    size_t size;
};

enum search_types
{
    NO_SEARCH = 0,
    EVENTS_SEARCH = 1,
    WAVEFORMS_SEARCH = 2
};

unsigned int terminate_flag = 0;

// Handle standard signals
// SIGTERM (from kill): terminates kindly forcing the status to the closing branch of the state machine.
// SIGINT (from ctrl-c): same behaviour as SIGTERM
// SIGHUP (from shell processes): same behaviour as SIGTERM

void signal_handler(int signum);

void print_usage(const char *name)
{
    printf("Usage: %s [options]\n", name);
    printf("\n");
    printf("Datastream filter that sorts the events in the data messages according to their timestamps.\n");
    printf("\n");
    printf("Optional arguments:\n");
    printf("\t-h: Display this message\n");
    printf("\t-v: Set verbose execution, repeating the option increases the verbosity level\n");
    printf("\n");
    printf("\t-A <address>: Input socket address, default: %s\n", defaults_abcd_data_address_sub);
    printf("\t-D <address>: Output socket address for sorted data, default: %s\n", defaults_cofi_coincidence_data_address);
    printf("\t-T <period>: Set base period in milliseconds, default: %d\n", defaults_cofi_base_period);

    return;
}

int compare_events(const void *a, const void *b);
void reallocate_array(size_t entries_number, size_t *previous_entries_number, void **entries_array, size_t sizeof_type);

int main(int argc, char *argv[])
{
    // Register the handler for SIGTERM (from kill), SIGINT (from ctrl-c)
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);

    unsigned int verbosity = 0;
    unsigned int base_period = defaults_cofi_base_period;
    char *input_address = defaults_abcd_data_address_sub;
    char *output_address = defaults_cofi_coincidence_data_address;

    int c = 0;
    while ((c = getopt(argc, argv, "hA:D:T:v")) != -1)
    {
        switch (c)
        {
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
            verbosity += 1;
            break;
        default:
            printf("Unknown command: %c", c);
            break;
        }
    }

    if (verbosity > 0)
    {
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
        printf("ERROR: ZeroMQ Error on input socket connection: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }

    const int osc = zmq_bind(output_socket, output_address);
    if (osc != 0)
    {
        printf("ERROR: ZeroMQ Error on output socket binding: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }

    // Subscribe to data topic
    zmq_setsockopt(input_socket, ZMQ_SUBSCRIBE, "data_abcd", strlen("data_abcd"));

    // Wait a bit to prevent the slow-joiner syndrome
    struct timespec slow_joiner_wait;
    slow_joiner_wait.tv_sec = defaults_all_slow_joiner_wait / 1000;
    slow_joiner_wait.tv_nsec = (defaults_all_slow_joiner_wait % 1000) * 1000000L;
    nanosleep(&slow_joiner_wait, NULL);

    struct timespec wait;
    wait.tv_sec = base_period / 1000;
    wait.tv_nsec = (base_period % 1000) * 1000000L;

    size_t previous_size_pointers_events_input = 0;
    size_t previous_size_buffer_output = 0;

    struct event_pointer *pointers_events_input = NULL;

    // This is a generic buffer used for the output of events, both event_PSD
    // as well as event_waveform. It is reused to reduce the number of memory
    // allocations.
    uint8_t *buffer_output = NULL;

    if (verbosity > 0)
    {
        printf("Allocating initial memory.\n");
    }

    // We start with a random initial guess for the sizes, that will be
    // increased when the input messages are determined.
    reallocate_array(INITIAL_BUFFER_SIZE,
                     &previous_size_pointers_events_input,
                     (void **)&pointers_events_input,
                     sizeof(struct event_pointer));

    reallocate_array(INITIAL_BUFFER_SIZE,
                     &previous_size_buffer_output,
                     (void **)&buffer_output,
                     sizeof(uint8_t));

    size_t loops_counter = 0;
    size_t msg_counter = 0;
    size_t msg_ID = 0;

    while (terminate_flag == 0)
    {
        char *topic;
        uint8_t *buffer_input;
        size_t size;

        // =====================================================================
        //  Messages reception
        // =====================================================================
        const int result = receive_byte_message(input_socket, &topic, (void **)(&buffer_input), &size, true, 0);

        if (result == EXIT_FAILURE)
        {
            printf("[%zu] ERROR: Some error occurred while receiving messages!!!\n", loops_counter);
        }
        else if (size == 0 && result == EXIT_SUCCESS)
        {
            if (verbosity > 3)
            {
                printf("[%zu] No message available\n", loops_counter);
            }
        }
        else if (size > 0 && result == EXIT_SUCCESS)
        {
            if (verbosity > 0)
            {
                printf("[%zu] Message received!!! (topic: %s)\n", loops_counter, topic);
            }

            const clock_t message_analysis_start = clock();

            int search_type = NO_SEARCH;
            size_t events_number = 0;

            if (strstr(topic, "data_abcd_events_v0") == topic)
            {
                // -------------------------------------------------------------
                //  event_PSD reading
                // -------------------------------------------------------------
                if (verbosity > 1)
                {
                    printf("Reading an event_PSD message\n");
                }

                search_type = EVENTS_SEARCH;

                events_number = size / sizeof(struct event_PSD);

                const struct event_PSD *events = (void *)buffer_input;

                if ((size % sizeof(struct event_PSD)) != 0)
                {
                    printf("ERROR: The buffer size is not a multiple of %zu\n", sizeof(struct event_PSD));
                }
                else
                {
                    reallocate_array(events_number,
                                     &previous_size_pointers_events_input,
                                     (void **)&pointers_events_input,
                                     sizeof(struct event_pointer));

                    if (!pointers_events_input)
                    {
                        printf("ERROR: Unable to allocate pointers_events_input buffer(s)\n");
                    }
                    else
                    {
                        // Storing the timestamps to the generic pointers
                        for (size_t index = 0; index < events_number; index++)
                        {
                            const struct event_PSD this_event = events[index];

                            pointers_events_input[index].index = index * sizeof(struct event_PSD);
                            pointers_events_input[index].timestamp = this_event.timestamp;
                            pointers_events_input[index].size = sizeof(struct event_PSD);
                        }
                    }
                }
            }
            else if (strstr(topic, "data_abcd_waveforms_v0") == topic)
            {
                // -------------------------------------------------------------
                //  event_waveform reading
                // -------------------------------------------------------------
                if (verbosity > 1)
                {
                    printf("Reading an event_waveform message\n");
                }

                search_type = WAVEFORMS_SEARCH;

                // Overestimating the number of event_waveforms by using the
                // buffer size and assuming that there are only empty waveforms
                const size_t size_estimation = size / waveform_header_size();

                if (verbosity > 1)
                {
                    printf("Previous size events input: %zu; size estimation: %zu, size: %zu.\n", previous_size_pointers_events_input, size_estimation, size);
                }

                reallocate_array(size_estimation,
                                 &previous_size_pointers_events_input,
                                 (void **)&pointers_events_input,
                                 sizeof(struct event_pointer));

                if (!pointers_events_input)
                {
                    printf("ERROR: Unable to allocate pointers_events_input buffer(s)\n");
                }
                else
                {
                    size_t events_counter = 0;
                    size_t input_offset = 0;

                    // Comparing also the events_counter against size to avoid
                    // accessing unauthorized memory areas
                    while (input_offset < size && events_counter < size_estimation)
                    {
                        const uint64_t timestamp = *((uint64_t *)(buffer_input + input_offset));
                        const uint32_t samples_number = *((uint32_t *)(buffer_input + input_offset + 9));
                        const uint8_t gates_number = *((uint8_t *)(buffer_input + input_offset + 13));

                        // Compute the waveform event size
                        const size_t this_size = waveform_header_size() + samples_number * sizeof(uint16_t) + gates_number * samples_number * sizeof(uint8_t);

                        pointers_events_input[events_counter].index = input_offset;
                        pointers_events_input[events_counter].timestamp = timestamp;
                        pointers_events_input[events_counter].size = this_size;

                        input_offset += this_size;

                        events_counter += 1;
                    }

                    // In this case something went very wrong and we set
                    // events_counter to zero, so the next steps are not done
                    if (size_estimation > events_counter)
                    {
                        events_number = events_counter;
                    }
                    else
                    {
                        events_number = 0;

                        printf("ERROR: Size estimation is less that then number of input events; size_estimation: %zu; events_counter: %zu;\n", size_estimation, events_counter);
                    }
                }
            }
            else
            {
                if (verbosity > 0)
                {
                    printf("WARNING: Forwarding unknown message\n");
                }

                send_byte_message(output_socket, topic, (void *)buffer_input, size, 0);
            }

            // =================================================================
            //  Sorting events' pointers
            // =================================================================
            if (verbosity > 1)
            {
                printf("Sorting pointers to events...\n");
            }
            const clock_t sorting_start = clock();
            qsort(pointers_events_input, events_number, sizeof(struct event_pointer), compare_events);
            const clock_t sorting_end = clock();

            if (verbosity > 1)
            {
                const float elaboration_time = (float)(sorting_end - sorting_start) / CLOCKS_PER_SEC * 1000;
                const float elaboration_speed_events = events_number / elaboration_time;

                printf("Sorting finished: events_number: %zu; elaboration_time: %f ms; elaboration_speed: %.1f kevts/s\n",
                       events_number, elaboration_time, elaboration_speed_events);
            }

            // =================================================================
            //  Output buffer computation
            // =================================================================
            reallocate_array(size,
                             &previous_size_buffer_output,
                             (void **)&buffer_output,
                             sizeof(uint8_t));

            size_t output_offset = 0;

            for (size_t index = 0; index < events_number; index++)
            {
                const struct event_pointer pointer_event = pointers_events_input[index];

                memcpy(buffer_output + output_offset, (buffer_input + pointer_event.index), pointer_event.size);
                output_offset += pointer_event.size;
            }

            // -----------------------------------------------------------------
            //  New topic computation
            // -----------------------------------------------------------------
            char new_topic[defaults_all_topic_buffer_size];
            if (search_type == EVENTS_SEARCH)
            {
                snprintf(new_topic, defaults_all_topic_buffer_size, "data_abcd_events_v0_s%zu", size);
            }
            else
            {
                snprintf(new_topic, defaults_all_topic_buffer_size, "data_abcd_waveforms_v0_s%zu", size);
            }

            send_byte_message(output_socket, new_topic, (void *)buffer_output, size, 0);
            msg_ID += 1;

            if (verbosity > 0)
            {
                printf("Sending message with topic: %s\n", new_topic);
            }

            const clock_t message_analysis_stop = clock();

            if (verbosity > 0)
            {
                const float elaboration_time = (float)(message_analysis_stop - message_analysis_start) / CLOCKS_PER_SEC * 1000;
                const float elaboration_speed_MB = size / elaboration_time * 1000.0 / 1024.0 / 1024.0;
                const float elaboration_speed_events = events_number / elaboration_time;

                printf("size: %zu; events_number: %zu; elaboration_time: %f ms; elaboration_speed: %.1f MBi/s, %.1f kevts/s\n",
                       size, events_number, elaboration_time, elaboration_speed_MB, elaboration_speed_events);
            }

            msg_counter += 1;

            // Remember to free buffers
            free(topic);
            free(buffer_input);
        }
        else
        {
            printf("[%zu] ERROR: What?!?!?!\n", loops_counter);
        }

        loops_counter += 1;

        // Putting a delay in order not to fill-up the queues
        nanosleep(&wait, NULL);

        if (verbosity > 3)
        {
            printf("loops_counter: %zu; msg_counter: %zu, msg_ID: %zu\n", loops_counter, msg_counter, msg_ID);
        }
    }

    // =========================================================================
    //  Closing up
    // =========================================================================

    if (pointers_events_input)
    {
        free(pointers_events_input);
    }
    if (buffer_output)
    {
        free(buffer_output);
    }

    // Wait a bit to allow the sockets to deliver
    nanosleep(&slow_joiner_wait, NULL);
    // usleep(defaults_all_slow_joiner_wait * 1000);

    const int ic = zmq_close(input_socket);
    if (ic != 0)
    {
        printf("ERROR: ZeroMQ Error on input socket close: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }

    const int occ = zmq_close(output_socket);
    if (occ != 0)
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

void signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM || signum == SIGHUP)
    {
        terminate_flag = 1;
    }
}

int compare_events(const void *a, const void *b)
{
    const struct event_pointer *event_a = (const struct event_pointer *)a;
    const struct event_pointer *event_b = (const struct event_pointer *)b;

    if (event_a->timestamp < event_b->timestamp)
    {
        return -1;
    }
    else if (event_a->timestamp > event_b->timestamp)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void reallocate_array(size_t entries_number, size_t *previous_entries_number, void **entries_array, size_t sizeof_type)
{
    if (entries_number > (*previous_entries_number))
    {
        *previous_entries_number = entries_number;

        void *new_elements_array = realloc((*entries_array), entries_number * sizeof_type);

        if (!new_elements_array)
        {
            printf("ERROR: reallocate_array(): Unable to allocate entries_array memory\n");

            (*entries_array) = NULL;
        }
        else
        {
            (*entries_array) = new_elements_array;
        }
    }
}
