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

#include <stdio.h>
// For nanosleep()
#include <time.h>
// Fot getopt
#include <getopt.h>
// For malloc
#include <stdlib.h>
// For memcpy
#include <string.h>

#include <zmq.h>

#include "defaults.h"
#include "socket_functions.h"

void print_usage(const char *name) {
    printf("Usage: %s [options] <file_name>\n", name);
    printf("\n");
    printf("System simulator that uses ABCD events files.\n");
    printf("\n");
    printf("Optional arguments:\n");
    printf("\t-h: Display this message\n");
    printf("\t-v: Set verbose execution\n");
    printf("\t-V: Set verbose execution with more details\n");
    printf("\t-S <address>: Status socket address, default: %s\n", defaults_abcd_status_address);
    printf("\t-D <address>: Data socket address, default: %s\n", defaults_abcd_data_address);
    printf("\t-T <period>: Set base period in milliseconds, default: %d\n", defaults_replay_base_period);
    printf("\t             For a very fast replay, 0 ms is also accepted.\n");
    printf("\t-B <size>: Events output buffers size, default: %d\n", defaults_all_topic_buffer_size);
    printf("\t-s <pknum>: Skip pknum packets, default: %d\n", defaults_replay_skip);

    return;
}

int main(int argc, char *argv[])
{
    unsigned int verbosity = 0;
    unsigned int base_period = defaults_replay_base_period;
    char *status_address = defaults_abcd_status_address;
    char *data_address = defaults_abcd_data_address;
    unsigned int skip_packets = defaults_replay_skip;
    size_t buffer_size = defaults_all_topic_buffer_size;

    int c = 0;
    while ((c = getopt(argc, argv, "hS:D:T:B:vVs:")) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'S':
                status_address = optarg;
                break;
            case 'D':
                data_address = optarg;
                break;
            case 'T':
                base_period = atoi(optarg);
                break;
            case 'B':
                buffer_size = atoi(optarg);
                break;
            case 's':
                skip_packets = atoi(optarg);
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

    if (optind >= argc)
    {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    const char *file_name = argv[optind];

    if (verbosity > 0) {
        printf("Status socket address: %s\n", status_address);
        printf("Data socket address: %s\n", data_address);
        printf("Buffer size: %zu\n", buffer_size);
        printf("File: %s\n", file_name);
        printf("Verbosity: %u\n", verbosity);
        printf("Base period: %u\n", base_period);
        printf("Packets to be skipped: %u\n", skip_packets);
    }

    // Creates a ØMQ context
    void *context = zmq_ctx_new();
    if (!context)
    {
        printf("ERROR: ZeroMQ Error on context creation");
        return EXIT_FAILURE;
    }

    void *status_socket = zmq_socket(context, ZMQ_PUB);
    if (!status_socket)
    {
        printf("ERROR: ZeroMQ Error on status socket creation\n");
        return EXIT_FAILURE;
    }

    void *data_socket = zmq_socket(context, ZMQ_PUB);
    if (!data_socket)
    {
        printf("ERROR: ZeroMQ Error on data socket creation\n");
        return EXIT_FAILURE;
    }

    const int s = zmq_bind(status_socket, status_address);
    if (s != 0)
    {
        printf("ERROR: ZeroMQ Error on status socket binding: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }

    const int d = zmq_bind(data_socket, data_address);
    if (d != 0)
    {
        printf("ERROR: ZeroMQ Error on data socket binding: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }

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

    FILE *in_file = fopen(file_name, "rb");

    if (!in_file)
    {
        printf("ERROR: Unable to open: %s\n", file_name);
    }
    else
    {
        // Compute the data topic
        char data_topic[defaults_all_topic_buffer_size];
        memset(data_topic, '\0', defaults_all_topic_buffer_size);

        // I am not sure if snprintf is standard or not, apprently it is in the C99 standard
        snprintf(data_topic, defaults_all_topic_buffer_size, "data_abcd_events_v0_s%zu", 16 * buffer_size);

        void *buffer = malloc(16 * buffer_size);

        if (buffer == NULL)
        {
            printf("ERROR: Unable to allocate buffer\n");
        }
        else
        {
            size_t bytes_counter = 0;
            size_t msg_id = 0;

            while ((!ferror(in_file)) && (!feof(in_file)))
            {
                const size_t bytes_read = fread(buffer, 16, buffer_size, in_file);

                bytes_counter += bytes_read * 16;

                if (verbosity > 1)
                {
                    printf("read: %zu, requested: %zu\n", bytes_read, buffer_size);

                    if (bytes_read < buffer_size)
                    {
                        printf("WARNING: Read bytes are less than requested\n");
                    }
                }

                if (verbosity > 0)
                {
                    printf("Topic [%zu]: %s\n", msg_id, data_topic);
                }

                if (msg_id < skip_packets)
                {
                    if (verbosity > 0)
                    {
                        printf("INFO: Skipping packet\n");
                    }
                }
                else
                {
                    send_byte_message(data_socket, data_topic, buffer, bytes_read * 16, verbosity);
                }

                if (verbosity > 0)
                {
                    printf("Read size: %zu B\n", bytes_counter);
                }

                if (msg_id >= skip_packets)
                {
                    // Putting a delay in order not to fill-up the queues
                    nanosleep(&wait, NULL);
                }

                msg_id += 1;
            }

            if (ferror(in_file))
            {
                printf("ERROR: Reading error\n");
            }

            if (feof(in_file) && (verbosity > 0))
            {
                printf("End of file\n");
            }

            free(buffer);
        }

        fclose(in_file);
    }

    // Wait a bit to allow the sockets to deliver
    nanosleep(&slow_joiner_wait, NULL);

    const int sc = zmq_close(status_socket);
    if (sc != 0)
    {
        printf("ERROR: ZeroMQ Error on status socket close: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }

    const int dc = zmq_close(data_socket);
    if (dc != 0)
    {
        printf("ERROR: ZeroMQ Error on data socket close: %s\n", zmq_strerror(errno));
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
