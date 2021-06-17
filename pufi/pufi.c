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
#include <jansson.h>

#include "defaults.h"
#include "events.h"
#include "socket_functions.h"
#include "point_in_polygon.h"

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
    printf("Usage: %s [options] <polygon_file_name>\n", name);
    printf("\n");
    printf("Datastream filter that selects data according to the PSD parameter:\n");
    printf("\n");
    printf("    PSD = Qtail / Qlong\n");
    printf("\n");
    printf("The selection is defined by a polygon on the (energy, PSD) plane.\n");
    printf("The polygon points shall be defined in a JSON file containing a signle array of x and y values\n");
    printf("Of course there should be at least three points to have a valid polygon.\n");
    printf("\n");
    printf("Optional arguments:\n");
    printf("\t-h: Display this message\n");
    printf("\t-v: Set verbose execution\n");
    printf("\t-V: Set verbose execution with more output\n");
    printf("\t-S <address>: SUB socket address, default: %s\n", defaults_abcd_data_address_sub);
    printf("\t-P <address>: PUB socket address, default: %s\n", defaults_pufi_data_address);
    printf("\t-T <period>: Set base period in milliseconds, default: %d\n", defaults_pufi_base_period);

    return;
}

int main(int argc, char *argv[])
{
    // Register the handler for SIGTERM (from kill), SIGINT (from ctrl-c)
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);

    unsigned int verbosity = 0;
    unsigned int base_period = defaults_pufi_base_period;
    char *input_address = defaults_abcd_data_address_sub;
    char *output_address = defaults_pufi_data_address;

    int c = 0;
    while ((c = getopt(argc, argv, "hS:P:T:w:l:r:n:vV")) != -1) {
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

    const int number_of_arguments = argc - optind;

    if (number_of_arguments < 1)
    {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    const char *polygon_file_name = argv[optind];

    json_error_t error;
    json_t *json_polygon = json_load_file(polygon_file_name, 0, &error);

    if (!json_polygon)
    {
        printf("ERROR: Parse error while reading polygon file: %s (source: %s, line: %d, column: %d, position: %d)\n", error.text, error.source, error.line, error.column, error.position);

        return EXIT_FAILURE;
    }

    if (!json_is_array(json_polygon))
    {
        printf("ERROR: JSON file does not contain a single array\n");

        json_decref(json_polygon);

        return EXIT_FAILURE;
    }

    const size_t number_of_points = json_array_size(json_polygon);

    struct Point_t *polygon = (struct Point_t *)malloc((number_of_points + 1) * sizeof(struct Point_t));

    if (polygon == NULL)
    {
        printf("ERROR: Unable to allocate memory for polygon\n");
        return EXIT_FAILURE;
    }

    size_t index;
    json_t *json_point;

    json_array_foreach(json_polygon, index, json_point) {
        polygon[index].x = json_number_value(json_object_get(json_point, "x"));
        polygon[index].y = json_number_value(json_object_get(json_point, "y"));
    }

    json_decref(json_polygon);

    // The winding number algorithm expects a closed loop so we repeat the first point
    polygon[number_of_points].x = polygon[0].x;
    polygon[number_of_points].y = polygon[0].y;

    // Compute the bounding box for a faster check up
    const struct BoundingBox_t bounding_box = compute_bounding_box(polygon, number_of_points);

    if (verbosity > 0) {
        printf("Input socket address: %s\n", input_address);
        printf("Output socket address: %s\n", output_address);
        printf("Verbosity: %u\n", verbosity);
        printf("Base period: %u\n", base_period);

        for (size_t i = 0; i < number_of_points + 1; i++)
        {
            printf("[i: %zu] point x: %f; y: %f;\n", i, polygon[i].x, polygon[i].y);
        }
        printf("Bounding box: x: [%f, %f]; y: [%f, %f];\n", bounding_box.top_left.x,
                                                            bounding_box.bottom_right.x,
                                                            bounding_box.bottom_right.y,
                                                            bounding_box.top_left.y);
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

                size_t bounding_box_selected_number = 0;
                size_t selected_number = 0;

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
                    // will be at most COINCIDENCE_defaults_all_topic_buffer_size * events_number coincidences.
                    struct event_PSD *selected_events = malloc(sizeof(struct event_PSD) * events_number);

                    if (selected_events == NULL)
                    {
                        printf("[%zu] ERROR: Unable to allocate output buffer\n", counter);
                    }
                    else
                    {
                        for (size_t i = 0; i < events_number; i++)
                        {
                            const struct event_PSD this_event = events[i];
                            //printf("i: %zu, ch: %d, t: %lu\n", i, this_event.channel, this_event.timestamp);

                            // We cast everything to double because the winding algorithm is
                            // expecting two variables of the same type.
                            const double energy = this_event.qlong;
                            const double PSD = (energy - this_event.qshort) / energy;
                            const struct Point_t point = {energy, PSD};

                            if (point_in_bounding_box(point, bounding_box))
                            {
                                bounding_box_selected_number++;

                                const int winding_number = point_winding_number(point, polygon, number_of_points);

                                if (verbosity > 1)
                                {
                                    printf("Point in bounding box i: %zu; energy: %f; PSD: %f; winding_number: %d;\n", i, energy, PSD, winding_number);
                                }

                                // The winding number is zero only when the point is outside the polygon
                                if (winding_number != 0)
                                {
                                    selected_events[selected_number] = this_event;

                                    selected_number++;

                                    if (verbosity > 1)
                                    {
                                        printf("Point selected!!!\n");
                                    }
                                }
                            }
                        }

                        const size_t output_size = selected_number * sizeof(struct event_PSD);

                        // Compute the new topic
                        char new_topic[defaults_all_topic_buffer_size];
                        // I am not sure if snprintf is standard or not, apprently it is in the C99 standard
                        snprintf(new_topic, defaults_all_topic_buffer_size, "data_abcd_events_v0_s%zu", output_size);

                        send_byte_message(output_socket, new_topic, (void *)selected_events, output_size, verbosity);
                        msg_ID += 1;

                        if (verbosity > 0)
                        {
                            printf("Sending message with topic: %s\n", new_topic);
                        }

                        free(selected_events);
                    }
                }

                const clock_t event_stop = clock();

                if (verbosity > 0)
                {
                    const float elaboration_time = (float)(event_stop - event_start) / CLOCKS_PER_SEC * 1000;
                    const float elaboration_speed = size / elaboration_time * 1000.0 / 1024.0 / 1024.0;

                    printf("size: %zu; events_number: %zu; bounding_box_selected_number: %zu; selected_number: %zu; ratio: %.2f%%; elaboration_time: %f ms; elaboration_speed: %f MBi/s\n", size, events_number, bounding_box_selected_number, selected_number, selected_number / (float)events_number * 100, elaboration_time, elaboration_speed);
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

    free(polygon);

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
