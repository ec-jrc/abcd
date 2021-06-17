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
#include <zlib.h>
#include <bzlib.h>

#include "defaults.h"
#include "socket_functions.h"
#include "utilities_functions.h"

unsigned int terminate_flag = 0;

char *rstrstr(char *big_string, char *small_string)
{
    size_t  big_string_len = strlen(big_string);
    size_t  small_string_len = strlen(small_string);
    char *s;

    if (small_string_len > big_string_len)
    {
        return NULL;
    }
    for (s = big_string + big_string_len - small_string_len; s >= big_string; --s)
    {
        if (strncmp(s, small_string, small_string_len) == 0)
        {
            return s;
        }
    }

    return NULL;
}

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
    char data_address[sizeof("[wxyz://255.255.255.255:65535]")];
    address_bind_to_connect(data_address, sizeof(data_address), defaults_abcd_data_address, defaults_abcd_ip);

    printf("Usage: %s [options]\n", name);
    printf("\n");
    printf("Datastream filter that compressed packets with the zlib or with bz2.\n");
    printf("\n");
    printf("Optional arguments:\n");
    printf("\t-h: Display this message\n");
    printf("\t-v: Set verbose execution\n");
    printf("\t-b: Enable usage of bz2\n");
    printf("\t-t <topic>: Topic to subscribe to, default: '%s'\n", defaults_gzad_topic_subscribe);
    printf("\t-S <address>: SUB socket address, default: %s\n", data_address);
    printf("\t-P <address>: PUB socket address, default: %s\n", defaults_gzad_data_address);
    printf("\t-T <period>: Set base period in milliseconds, default: %d\n", defaults_gzad_base_period);

    return;
}

int main(int argc, char *argv[])
{
    // Register the handler for SIGTERM (from kill), SIGINT (from ctrl-c)
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);

    unsigned int verbosity = 0;
    unsigned int base_period = defaults_gzad_base_period;
    char *input_address = defaults_abcd_data_address_sub;
    char *output_address = defaults_gzad_data_address;
    char *subscription_topic = defaults_gzad_topic_subscribe;
    bool use_bz2 = false;

    int c = 0;
    while ((c = getopt(argc, argv, "hbt:S:P:T:v")) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'b':
                use_bz2 = true;
                break;
            case 't':
                subscription_topic = optarg;
                break;
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
    zmq_setsockopt(input_socket, ZMQ_SUBSCRIBE, subscription_topic, strlen(subscription_topic));

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

            const clock_t event_start = clock();

            uint64_t output_size = 0;

            uint8_t *output_buffer = malloc(size * sizeof(uint8_t));

            if (output_buffer == NULL)
            {
                printf("[%zu] ERROR: Unable to allocate output buffer\n", counter);
            }
            else
            {
                if (use_bz2 == false)
                {
                    // Preparing the struct that the library uses as input
                    z_stream zipped_stream;

                    zipped_stream.zalloc = Z_NULL;
                    zipped_stream.zfree = Z_NULL;
                    zipped_stream.opaque = Z_NULL;
                    zipped_stream.data_type = Z_BINARY;

                    zipped_stream.avail_in = (uInt)size;
                    zipped_stream.next_in = (Bytef *)input_buffer;
                    zipped_stream.avail_out = (uInt)size;
                    zipped_stream.next_out = (Bytef *)output_buffer;

                    deflateInit(&zipped_stream, Z_BEST_SPEED);
                    deflate(&zipped_stream, Z_FINISH);

                    output_size = zipped_stream.total_out;

                    deflateEnd(&zipped_stream);
                }
                else
                {
                    // Preparing the struct that the library uses as input
                    bz_stream zipped_stream;

                    zipped_stream.bzalloc = NULL;
                    zipped_stream.bzfree = NULL;
                    zipped_stream.opaque = NULL;

                    zipped_stream.avail_in = size;
                    zipped_stream.next_in = input_buffer;
                    zipped_stream.avail_out = size;
                    zipped_stream.next_out = (char*)output_buffer;

                    BZ2_bzCompressInit(&zipped_stream, 9, verbosity, 0);
                    BZ2_bzCompress(&zipped_stream, BZ_FINISH);

                    // Apparently these two are both valid
                    //output_size = size - zipped_stream.avail_out;
                    output_size = (((size_t)zipped_stream.total_out_hi32) << 32) | zipped_stream.total_out_lo32;

                    BZ2_bzCompressEnd(&zipped_stream);
                }

                char topic_no_size[defaults_all_topic_buffer_size];

                // Find the size portion of the topic
                char *size_position = rstrstr(topic, "_s");
                size_t size_index = size_position - topic;

                if (!size_position)
                {
                    printf("[%zu] WARNING: Unable to find size in topic\n", counter);
                    size_index = strlen(topic);
                }

                strncpy(topic_no_size, topic, size_index);
                topic_no_size[size_index] = '\0';

                // Compute the new topic
                char new_topic[defaults_all_topic_buffer_size];

                const char *compression = use_bz2 ? "bz2_" : "zlib_"; 

                // I am not sure if snprintf is standard or not, apprently it is in the C99 standard
                snprintf(new_topic,
                         defaults_all_topic_buffer_size,
                         "compressed_%s%s_s%" PRIu64,
                         compression, topic_no_size, output_size);

                send_byte_message(output_socket, new_topic, (void *)output_buffer, output_size, verbosity);
                msg_ID += 1;

                if (verbosity > 0)
                {
                    printf("Sending message with topic: %s\n", new_topic);
                }

                free(output_buffer);
            }

            const clock_t event_stop = clock();

            if (verbosity > 0)
            {
                const float elaboration_time = (float)(event_stop - event_start) / CLOCKS_PER_SEC * 1000;
                const float elaboration_speed = size / elaboration_time * 1000.0 / 1024.0 / 1024.0;

                printf("size: %zu; output_size: %" PRIu64 "; ratio: %.1f%%; elaboration_time: %f ms; elaboration_speed: %f MBi/s\n", size, output_size, ((float)output_size) / size * 100, elaboration_time, elaboration_speed);
                //printf("Elaboration time: %f ms\n", (float)(event_stop - event_start) / CLOCKS_PER_SEC * 1000);
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

        if (verbosity > 1)
        {
            printf("counter: %zu; msg_counter: %zu, msg_ID: %zu\n", counter, msg_counter, msg_ID);
        }
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
