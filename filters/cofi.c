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
    uint8_t channel;
    uint8_t group_counter;
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
    printf("Usage: %s [options] <reference_channels>\n", name);
    printf("\n");
    printf("Datastream filter that selects the events that are in coincidence with a set of channels in a defined time window.\n");
    printf("\n");
    printf("Optional arguments:\n");
    printf("\t-h: Display this message\n");
    printf("\t-v: Set verbose execution, repeating the option increases the verbosity level\n");
    printf("\n");
    printf("\t-A <address>: Input socket address, default: %s\n", defaults_abcd_data_address_sub);
    printf("\t-D <address>: Output socket address for coincidence data, default: %s\n", defaults_cofi_coincidence_data_address);
    printf("\t-N <address>: Output socket address for anti-coincidence data, default: %s\n", defaults_cofi_anticoincidence_data_address);
    printf("\t-T <period>: Set base period in milliseconds, default: %d\n", defaults_cofi_base_period);
    printf("\n");
    printf("\t-w <coincidence_window>: Symmetric coincidence window in nanoseconds\n");
    printf("\t-L <left_coincidence_edge>: Left edge of the coincidence window in nanoseconds relative to the reference pulse, default: %f\n", -1 * defaults_cofi_coincidence_window_left);
    printf("\t-l <left_coincidence_window>: Left coincidence window width in nanoseconds extending to negative times relative to the reference pulse, default: %f\n", defaults_cofi_coincidence_window_left);
    printf("\t-r <right_coincidence_window>: Right coincidence window width in nanoseconds relative to the reference pulse, default: %f\n", defaults_cofi_coincidence_window_right);
    printf("\n");
    printf("\t-m <multiplicity>: Event minimum multiplicity excluding the reference channel, default: %u\n", defaults_cofi_multiplicity);
    printf("\t-n <ns_per_sample>: Nanoseconds per sample, default: %f\n", defaults_cofi_ns_per_sample);
    printf("\t-k: Enable keep reference event, even if there are no coincidences.\n");
    printf("\t-a: Enable the calculation of anticoincidences and the creation of the corresponding socket.\n");

    return;
}

bool in_array(int channel, int *selected_channels, size_t number_of_channels);
int compare_events(const void *a, const void *b);
void reallocate_array(size_t entries_number, size_t *previous_entries_number, void **entries_array, size_t sizeof_type);

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
    while ((c = getopt(argc, argv, "hA:D:N:T:w:l:r:L:n:m:kav")) != -1)
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
            verbosity += 1;
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

    if (!reference_channels)
    {
        printf("ERROR: Unable to allocate reference_channels array\n");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < number_of_references; i++)
    {
        reference_channels[i] = atoi(argv[optind + i]);
    }

    const int64_t left_coincidence_window = (int64_t)(left_coincidence_window_ns / ns_per_sample);
    const int64_t right_coincidence_window = (int64_t)(right_coincidence_window_ns / ns_per_sample);

    if (verbosity > 0)
    {
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

    void *output_socket_coincidences = zmq_socket(context, ZMQ_PUB);
    if (!output_socket_coincidences)
    {
        printf("ERROR: ZeroMQ Error on output socket creation for coincidences\n");
        return EXIT_FAILURE;
    }

    void *output_socket_anticoincidences = NULL;

    if (enable_anticoincidences)
    {
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

    if (enable_anticoincidences)
    {
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

    struct timespec wait;
    wait.tv_sec = base_period / 1000;
    wait.tv_nsec = (base_period % 1000) * 1000000L;

    size_t previous_size_pointers_events_input = 0;
    size_t previous_size_pointers_events_output = 0;
    size_t previous_size_selection_anticoincidence = 0;
    size_t previous_size_buffer_output = 0;

    struct event_pointer *pointers_events_input = NULL;
    struct event_pointer *pointers_events_output = NULL;
    bool *selection_anticoincidence = NULL;

    // This is a generic buffer used for the output of events, both event_PSD
    // as well as event_waveform. It is reused to reduce the number of memory
    // allocations.
    uint8_t *buffer_output = NULL;

    if (verbosity > 0)
    {
        printf("Allocating initial memory for coincidence data.\n");
    }

    // We start with a random initial guess for the sizes, that will be
    // increased when the coincidences are determined.
    reallocate_array(INITIAL_BUFFER_SIZE,
                     &previous_size_pointers_events_input,
                     (void **)&pointers_events_input,
                     sizeof(struct event_pointer));
    reallocate_array(INITIAL_BUFFER_SIZE,
                     &previous_size_pointers_events_output,
                     (void **)&pointers_events_output,
                     sizeof(struct event_pointer));

    reallocate_array(INITIAL_BUFFER_SIZE,
                     &previous_size_buffer_output,
                     (void **)&buffer_output,
                     sizeof(uint8_t));

    if (enable_anticoincidences)
    {
        if (verbosity > 0)
        {
            printf("Allocating initial memory for anticoincidence data.\n");
        }

        reallocate_array(INITIAL_BUFFER_SIZE,
                         &previous_size_selection_anticoincidence,
                         (void **)&selection_anticoincidence,
                         sizeof(bool));
    }

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
                            pointers_events_input[index].channel = this_event.channel;
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
                        const uint8_t channel = *((uint8_t *)(buffer_input + input_offset + 8));
                        const uint32_t samples_number = *((uint32_t *)(buffer_input + input_offset + 9));
                        const uint8_t gates_number = *((uint8_t *)(buffer_input + input_offset + 13));

                        // Compute the waveform event size
                        const size_t this_size = waveform_header_size() + samples_number * sizeof(uint16_t) + gates_number * samples_number * sizeof(uint8_t);

                        pointers_events_input[events_counter].index = input_offset;
                        pointers_events_input[events_counter].timestamp = timestamp;
                        pointers_events_input[events_counter].channel = channel;
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

                send_byte_message(output_socket_coincidences, topic, (void *)buffer_input, size, 0);
            }

            // -----------------------------------------------------------------
            //  Allocation of output buffer
            // -----------------------------------------------------------------

            // At most we can have an events_number**2 number of coincidences in
            // the case that the coincidence window is so big that every event
            // is in coincidence with every other event.
            // In that case all the events would be copied to the output
            // buffer at each reference event.
            // That size would be too much for a buffer, so we just
            // use a bigger buffer selected in the settings.
            reallocate_array(events_number * defaults_cofi_coincidence_buffer_multiplier,
                             &previous_size_pointers_events_output,
                             (void **)&pointers_events_output,
                             sizeof(struct event_pointer));

            if (verbosity > 1)
            {
                printf("Previous size events input: %zu; previous size events output: %zu; events number: %zu; size: %zu.\n", previous_size_pointers_events_input, previous_size_pointers_events_output, events_number, size);
            }

            if (enable_anticoincidences)
            {
                reallocate_array(events_number,
                                 &previous_size_selection_anticoincidence,
                                 (void **)&selection_anticoincidence,
                                 sizeof(bool));

                for (size_t index = 0; index < events_number; index++)
                {
                    selection_anticoincidence[index] = true;
                }
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

            size_t output_size = 0;

            // =================================================================
            //  Coincidences determination
            // =================================================================
            size_t index_coincidence_group_start = 0;
            for (size_t index = 0; index < events_number; index++)
            {
                const struct event_pointer this_event = pointers_events_input[index];

                if (in_array(this_event.channel, reference_channels, number_of_references))
                {
                    // Just renaming the variable since now we know that it is
                    // from a reference channel
                    const struct event_pointer reference_event = this_event;
                    // Coincidence windows might be bigger than timestamps and
                    // thus edges could be negative, so we must use signed ints,
                    // this is going to bother the compiler because the
                    // timestamps are unsigned.
                    const int64_t left_edge = reference_event.timestamp - left_coincidence_window;
                    const int64_t right_edge = reference_event.timestamp + right_coincidence_window;

                    size_t coincidence_group_found_coincidences = 0;

                    // ---------------------------------------------------------
                    //  Search backward in time
                    // ---------------------------------------------------------
                    for (int64_t sub_index = (index - 1); sub_index > 0; sub_index--)
                    {
                        const struct event_pointer that_event = pointers_events_input[sub_index];

                        // The timestamp condition is probably the most
                        // stringent and thus checking it first
                        if (left_edge < that_event.timestamp && that_event.timestamp < right_edge)
                        {
                            if (!in_array(that_event.channel, reference_channels, number_of_references))
                            {
                                // Increase already the coincidence_group_found_coincidences counter, so
                                // the first event in the output coincidence group is left
                                // empty at this stage. It will be filled with the reference
                                // event afterwards.
                                coincidence_group_found_coincidences += 1;

                                if (verbosity > 1)
                                {
                                    printf("  Backward coincidence!!!! index: %zu, sub_index: %" PRId64 ", channel: %" PRIu8 ", timestamps: reference: %" PRIu64 ", that: %" PRIu64 ", difference: %" PRId64 "\n", index, sub_index, that_event.channel, reference_event.timestamp, that_event.timestamp, ((int64_t)that_event.timestamp) - (int64_t)reference_event.timestamp);
                                }

                                // Making sure that we do not access memory that
                                // we are not supposed to.
                                if ((index_coincidence_group_start + coincidence_group_found_coincidences) < defaults_cofi_coincidence_buffer_multiplier * events_number)
                                {
                                    pointers_events_output[index_coincidence_group_start + coincidence_group_found_coincidences] = that_event;
                                    output_size += that_event.size;
                                }

                                if (enable_anticoincidences)
                                {
                                    selection_anticoincidence[sub_index] = false;
                                }
                            }
                        }
                        else
                        {
                            break;
                        }
                    }

                    // ---------------------------------------------------------
                    //  Search forward in time
                    // ---------------------------------------------------------
                    for (int64_t sub_index = (index + 1); sub_index < (int64_t)events_number; sub_index++)
                    {
                        const struct event_pointer that_event = pointers_events_input[sub_index];

                        if (left_edge < that_event.timestamp && that_event.timestamp < right_edge)
                        {
                            if (!in_array(that_event.channel, reference_channels, number_of_references))
                            {
                                // See previous comment
                                coincidence_group_found_coincidences += 1;

                                if (verbosity > 1)
                                {
                                    printf("  Forward coincidence!!!! index: %zu, sub_index: %" PRId64 ", channel: %" PRIu8 ", timestamps: reference: %" PRIu64 ", that: %" PRIu64 ", difference: %" PRId64 "\n", index, sub_index, that_event.channel, reference_event.timestamp, that_event.timestamp, ((int64_t)that_event.timestamp) - (int64_t)reference_event.timestamp);
                                }

                                if ((index_coincidence_group_start + coincidence_group_found_coincidences) < defaults_cofi_coincidence_buffer_multiplier * events_number)
                                {
                                    pointers_events_output[index_coincidence_group_start + coincidence_group_found_coincidences] = that_event;
                                    output_size += that_event.size;
                                }

                                if (enable_anticoincidences)
                                {
                                    selection_anticoincidence[sub_index] = false;
                                }
                            }
                        }
                        else
                        {
                            break;
                        }
                    }

                    if (verbosity > 1 && coincidence_group_found_coincidences > 0)
                    {
                        printf("    Event multiplicity: %zu%s\n", coincidence_group_found_coincidences, (coincidence_group_found_coincidences >= multiplicity) ? " SELECTING GROUP!!!" : "");
                    }

                    // ---------------------------------------------------------
                    //  Storing the reference event to the coincidence group
                    // ---------------------------------------------------------
                    if (coincidence_group_found_coincidences >= multiplicity)
                    {
                        // Sorting the coincidence group only
                        qsort(pointers_events_output + (index_coincidence_group_start + 1), coincidence_group_found_coincidences, sizeof(struct event_pointer), compare_events);

                        if (index_coincidence_group_start < defaults_cofi_coincidence_buffer_multiplier * events_number)
                        {
                            pointers_events_output[index_coincidence_group_start] = reference_event;
                            output_size += reference_event.size;

                            if (coincidence_group_found_coincidences > 0xFF)
                            {
                                pointers_events_output[index_coincidence_group_start].group_counter = 0xFF;
                            }
                            else
                            {
                                pointers_events_output[index_coincidence_group_start].group_counter = (uint8_t)coincidence_group_found_coincidences;
                            }

                            index_coincidence_group_start += (1 + coincidence_group_found_coincidences);
                        }

                        if (enable_anticoincidences)
                        {
                            selection_anticoincidence[index] = false;
                        }
                    }
                    else if (keep_reference_event)
                    {
                        if (index_coincidence_group_start < defaults_cofi_coincidence_buffer_multiplier * events_number)
                        {
                            pointers_events_output[index_coincidence_group_start] = reference_event;
                            output_size += reference_event.size;

                            pointers_events_output[index_coincidence_group_start].group_counter = 0;
                            index_coincidence_group_start += 1;
                        }

                        if (enable_anticoincidences)
                        {
                            selection_anticoincidence[index] = false;
                        }
                    }
                }
            }

            // =================================================================
            //  Output buffer computation for coincidence data
            // =================================================================
            // Renaming variable for convenience
            const size_t coincidences_number = index_coincidence_group_start;

            if (verbosity > 1)
            {
                printf("events_number: %zu; coincidences_number: %zu; output_size: %zu; previous_size_buffer_output: %zu;\n",
                       events_number, coincidences_number, output_size, previous_size_buffer_output);
            }

            reallocate_array(output_size,
                             &previous_size_buffer_output,
                             (void **)&buffer_output,
                             sizeof(uint8_t));

            if (search_type == EVENTS_SEARCH)
            {
                // For events we need to manage the group_counter member so we
                // need to modify the events. We cannot simply memcpy() them to
                // the output buffer.
                const struct event_PSD *events = (void *)buffer_input;

                size_t output_offset = 0;

                for (size_t index = 0; index < coincidences_number; index++)
                {
                    const struct event_pointer pointer_event = pointers_events_output[index];
                    // The index of the pointer is the global index of the
                    // buffer_input so for uint8_t values. We must convert them
                    const size_t pointer_index = pointer_event.index / sizeof(struct event_PSD);
                    struct event_PSD this_event = events[pointer_index];
                    this_event.group_counter = pointer_event.group_counter;

                    memcpy(buffer_output + output_offset, &this_event, sizeof(struct event_PSD));
                    output_offset += pointer_event.size;
                }
            }
            else
            {
                size_t output_offset = 0;

                for (size_t index = 0; index < coincidences_number; index++)
                {
                    const struct event_pointer pointer_event = pointers_events_output[index];

                    memcpy(buffer_output + output_offset, (buffer_input + pointer_event.index), pointer_event.size);
                    output_offset += pointer_event.size;
                }
            }

            // -----------------------------------------------------------------
            //  New topic computation
            // -----------------------------------------------------------------
            char new_topic[defaults_all_topic_buffer_size];
            if (search_type == EVENTS_SEARCH)
            {
                snprintf(new_topic, defaults_all_topic_buffer_size, "data_abcd_events_v0_s%zu", output_size);
            }
            else
            {
                snprintf(new_topic, defaults_all_topic_buffer_size, "data_abcd_waveforms_v0_s%zu", output_size);
            }

            send_byte_message(output_socket_coincidences, new_topic, (void *)buffer_output, output_size, 0);
            msg_ID += 1;

            if (verbosity > 0)
            {
                printf("Sending message with coincidences with topic: %s\n", new_topic);
            }

            // =================================================================
            //  Output buffer computation for anticoincidence data
            // =================================================================
            if (enable_anticoincidences)
            {
                // Make sure that the anticoincidences buffer is big enough.
                // Since the anticoincidences are certainly a subset of the
                // original events, we can use the initial size
                reallocate_array(size,
                                 &previous_size_buffer_output,
                                 (void **)&buffer_output,
                                 sizeof(uint8_t));

                size_t output_offset = 0;
                size_t found_anticoincidences = 0;

                for (size_t index = 0; index < events_number; index++)
                {
                    const struct event_pointer pointer_event = pointers_events_input[index];

                    if (selection_anticoincidence[index] == true)
                    {
                        memcpy(buffer_output + output_offset, (buffer_input + pointer_event.index), pointer_event.size);
                        output_offset += pointer_event.size;

                        if (verbosity > 2)
                        {
                            printf(" anticoincidence: index: %zu, found: %zu; channel: %" PRIu8 ", timestamp: %" PRIu64 "\n",
                                   index, found_anticoincidences, pointer_event.channel, pointer_event.timestamp);
                        }

                        found_anticoincidences += 1;
                    }
                }

                if (verbosity > 0)
                {
                    printf("events_number: %zu; coincidences_number: %zu; anticoincidences_number: %zu;\n",
                           events_number, coincidences_number, found_anticoincidences);
                }

                if (found_anticoincidences > 0)
                {
                    // ---------------------------------------------------------
                    //  New topic computation
                    // ---------------------------------------------------------
                    char new_topic[defaults_all_topic_buffer_size];
                    if (search_type == EVENTS_SEARCH)
                    {
                        snprintf(new_topic, defaults_all_topic_buffer_size, "data_abcd_events_v0_s%zu", output_offset);
                    }
                    else
                    {
                        snprintf(new_topic, defaults_all_topic_buffer_size, "data_abcd_waveforms_v0_s%zu", output_offset);
                    }

                    send_byte_message(output_socket_anticoincidences, new_topic, (void *)buffer_output, output_offset, 0);
                    msg_ID += 1;

                    if (verbosity > 0)
                    {
                        printf("Sending message with anticoincidences with topic: %s\n", new_topic);
                    }
                }
            }

            const clock_t message_analysis_stop = clock();

            if (verbosity > 0)
            {
                const float elaboration_time = (float)(message_analysis_stop - message_analysis_start) / CLOCKS_PER_SEC * 1000;
                const float elaboration_speed_MB = size / elaboration_time * 1000.0 / 1024.0 / 1024.0;
                const float elaboration_speed_events = events_number / elaboration_time;

                printf("size: %zu; events_number: %zu; coincidences_number: %zu; ratio: %.2f%%; elaboration_time: %f ms; elaboration_speed: %.1f MBi/s, %.1f kevts/s\n",
                       size, events_number, coincidences_number, coincidences_number / (float)events_number * 100, elaboration_time, elaboration_speed_MB, elaboration_speed_events);
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
    if (pointers_events_output)
    {
        free(pointers_events_output);
    }
    if (selection_anticoincidence)
    {
        free(selection_anticoincidence);
    }

    if (buffer_output)
    {
        free(buffer_output);
    }

    if (reference_channels)
    {
        free(reference_channels);
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

    const int occ = zmq_close(output_socket_coincidences);
    if (occ != 0)
    {
        printf("ERROR: ZeroMQ Error on output socket for coincidences close: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }

    if (enable_anticoincidences)
    {
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

void signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM || signum == SIGHUP)
    {
        terminate_flag = 1;
    }
}

bool in_array(int channel, int *selected_channels, size_t number_of_channels)
{
    for (size_t i = 0; i < number_of_channels; i++)
    {
        if (channel == selected_channels[i])
        {
            return true;
        }
    }

    return false;
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
