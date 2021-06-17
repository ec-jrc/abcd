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

#include <stdio.h>
// Fot getopt
#include <getopt.h>
// For malloc
#include <stdlib.h>
// For memcpy
#include <string.h>

#include "defaults.h"

void print_usage(const char *name) {
    printf("Usage: %s [options] <file_name>\n", name);
    printf("\n");
    printf("Converts ABCD raw files to events files.\n");
    printf("The output file name is the same as the input with the adr extension changed.\n");
    printf("\n");
    printf("Optional arguments:\n");
    printf("\t-h: Display this message\n");
    printf("\t-o <file_name>: Set output file name.\n");
    printf("\t-v: Set verbose execution\n");
    printf("\t-V: Set more verbose execution\n");
    printf("\t-s <pknum>: Skip pknum packets, default: %d\n", defaults_replay_skip);

    return;
}

int main(int argc, char *argv[])
{
    unsigned int verbosity = 0;
    unsigned int skip_packets = defaults_replay_skip;
    char *user_output_file_name = NULL;

    int c = 0;
    while ((c = getopt(argc, argv, "ho:vVs:")) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 's':
                skip_packets = atoi(optarg);
                break;
            case 'o':
                user_output_file_name = optarg;
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
    char *output_file_name = NULL;

    if (!user_output_file_name) {
        // Looking for the extension of the file_name by searching for the last '.'
        char *extension_pointer = strrchr(file_name,'.');

        size_t basename_size = 0;

        if (extension_pointer == NULL)
        {
            basename_size = strlen(file_name);
        }
        else
        {
            basename_size = (extension_pointer - file_name);
        }

        output_file_name = calloc(basename_size + 4 + 1, sizeof(char));

        if (!output_file_name) {
            printf("ERROR: Unable to allocate output file name.\n");

            return EXIT_SUCCESS;
        }

        memcpy(output_file_name, file_name, basename_size * sizeof(char));
        memcpy(output_file_name + basename_size, ".ade", strlen(".ade"));
    }
    else
    {
        output_file_name = calloc(strlen(user_output_file_name) + 1, sizeof(char));

        if (!output_file_name) {
            printf("ERROR: Unable to allocate output file name.\n");

            return EXIT_SUCCESS;
        }

        memcpy(output_file_name, user_output_file_name, strlen(user_output_file_name));
    }

    if (verbosity > 0) {
        printf("File: %s\n", file_name);
        printf("Output file: %s\n", output_file_name);
        printf("Verbosity: %u\n", verbosity);
        printf("Packets to be skipped: %u\n", skip_packets);
    }

    FILE *in_file = fopen(file_name, "rb");
    FILE *out_file = fopen(output_file_name, "wb");

    if (!in_file)
    {
        printf("ERROR: Unable to open: %s\n", file_name);
    }

    if (!out_file)
    {
        printf("ERROR: Unable to open: %s\n", output_file_name);
    }

    if (in_file && out_file)
    {
        char topic_buffer[defaults_all_topic_buffer_size];
        size_t topics_counter = 0;
        size_t partial_counter = 0;
        size_t bytes_counter = 0;
        size_t status_packets_counter = 0;
        size_t data_events_packets_counter = 0;
        size_t data_waveforms_packets_counter = 0;
        size_t zipped_packets_counter = 0;
        size_t unknown_packets_counter = 0;

        memset(topic_buffer, '\0', defaults_all_topic_buffer_size);

        int c;
        while ((c = fgetc(in_file)) != EOF)
        {
            bytes_counter += 1;

            if (c != ' ')
            {
                if (partial_counter < defaults_all_topic_buffer_size)
                {
                    topic_buffer[partial_counter] = (char)c;
                    partial_counter += 1;
                }
                else
                {
                    printf("ERROR: Topic too long: %zu\n", partial_counter);
                    return EXIT_SUCCESS;
                }
            }
            else
            {
                if (verbosity > 0)
                {
                    printf("Topic [%zu]: %s\n", topics_counter, topic_buffer);
                }

                // Looking for the size of the message by searching for the last '_'
                char *size_pointer = strrchr(topic_buffer,'_');
                size_t size = 0;
                if (size_pointer == NULL)
                {
                    printf("ERROR: Unable to find message size, topic: %s\n", topic_buffer);
                }
                else
                {
                    size = atoi(size_pointer + 2);
                }

                if (verbosity > 1)
                {
                    printf("Message size: %zu\n", size);
                }

                void *buffer = malloc(sizeof(char) * size);

                const size_t bytes_read = fread(buffer, sizeof(char), size, in_file);

                bytes_counter += sizeof(char) + size;

                if (verbosity > 1)
                {
                    printf("read: %zu, requested: %zu\n", bytes_read, size * sizeof(char));
                }

                if (bytes_read != (sizeof(char) * size))
                {
                    printf("ERROR: Unable to read all the requested bytes: read: %zu, requested: %zu\n", bytes_read, size * sizeof(char));
                }


                const int status_compared = strncmp(topic_buffer, "status", strlen("status"));
                const int events_compared = strncmp(topic_buffer, "events", strlen("events"));
                const int data_events_compared = strncmp(topic_buffer, "data_abcd_events", strlen("data_abcd_events"));
                const int data_waveforms_compared = strncmp(topic_buffer, "data_abcd_waveforms", strlen("data_abcd_waveforms"));
                const int zipped_compared = strncmp(topic_buffer, "compressed", strlen("compressed"));

                if (verbosity > 1)
                {
                    printf("status_compared: %d; events_compared: %d; data_events_compared: %d; data_waveforms_compared: %d; zipped_compared: %d\n", status_compared, events_compared, data_events_compared, data_waveforms_compared, zipped_compared);
                }

                // If it is a status-like packet update counter and do nothing
                if ((status_compared == 0 || events_compared == 0) &&
                     data_events_compared != 0 &&
                     data_waveforms_compared != 0 &&
                     zipped_compared != 0)
                {
                    status_packets_counter += 1;

                    if (topics_counter < skip_packets && verbosity > 0)
                    {
                        printf("INFO: Skipping packet\n");
                    }
                }
                // If it is a data packet update counter and write it to file
                else if (status_compared != 0 &&
                         events_compared != 0 &&
                         data_events_compared != 0 &&
                         data_waveforms_compared == 0 &&
                         zipped_compared != 0)
                {
                    data_waveforms_packets_counter += 1;

                    if (topics_counter < skip_packets && verbosity > 0)
                    {
                        printf("INFO: Skipping packet\n");
                    }
                }
                // If it is a data packet update counter and write it to file
                else if (status_compared != 0 &&
                         events_compared != 0 &&
                         data_events_compared == 0 &&
                         data_waveforms_compared != 0 &&
                         zipped_compared != 0)
                {
                    data_events_packets_counter += 1;

                    if (topics_counter < skip_packets)
                    {
                        if (verbosity > 0)
                        {
                            printf("INFO: Skipping packet\n");
                        }
                    }
                    else
                    {
                        fwrite(buffer, sizeof(char), size, out_file);
                    }
                }
                // If it is a compressed packet update counter and do nothing
                else if (status_compared != 0 &&
                         events_compared != 0 &&
                         data_events_compared != 0 &&
                         data_waveforms_compared != 0 &&
                         zipped_compared == 0)
                {
                    zipped_packets_counter += 1;

                    if (topics_counter < skip_packets && verbosity > 0)
                    {
                        printf("INFO: Skipping packet\n");
                    }
                }
                else
                {
                    unknown_packets_counter += 1;

                    if (verbosity > 0)
                    {
                        printf("WARNING: Unknown packet type, skipping it.\n");
                    }
                }

                free(buffer);
                memset(topic_buffer, '\0', defaults_all_topic_buffer_size);

                partial_counter = 0;
                topics_counter += 1;

                if (verbosity > 0)
                {
                    printf("packets: %zu; status packets: %zu; data events packets: %zu; data waveforms packets: %zu; compressed packets: %zu; unkown packets: %zu\n", topics_counter, status_packets_counter, data_events_packets_counter, data_waveforms_packets_counter, zipped_packets_counter, unknown_packets_counter);
                }

                if (verbosity > 0)
                {
                    printf("Read size: %zu B\n", bytes_counter);
                }
            }

            if (verbosity > 2)
            {
                printf("partial_counter: %zu; c: %c\n", partial_counter, c);
            }
        }
    }

    if (in_file)
    {
        if (verbosity > 1) {
            printf("Closing: %s\n", file_name);
        }
        fclose(in_file);
    }

    if (out_file)
    {
        if (verbosity > 1) {
            printf("Closing: %s\n", output_file_name);
        }
        fclose(out_file);
    }

    return EXIT_SUCCESS;
}
