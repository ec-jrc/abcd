/*
 * (C) Copyright 2019 Cristiano Lino Fontana
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
// For getopt
#include <getopt.h>
#include <stdint.h>
#include <inttypes.h>
// For EXIT_SUCCESS
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "events.h"

// Function to parse the command line options, that is defined after the main()
int parse_command_line(int argc, char *argv[], unsigned int *verbosity, char **output_file_name, char **input_file_name);

int main(int argc, char *argv[])
{
    // Command line options parsing
    //
    unsigned int verbosity = 0;
    char *output_file_name = NULL;
    char *input_file_name = NULL;

    if (parse_command_line(argc, argv, &verbosity, &output_file_name, &input_file_name) == EXIT_FAILURE) {
       return EXIT_FAILURE;
    }

    // Opening of files
    bool error_flag = false;

    FILE *in_file = fopen(input_file_name, "rb");

    if (!in_file)
    {
        fprintf(stderr, "ERROR: Unable to open: %s\n", input_file_name);
        error_flag = true;
    }

    FILE *out_file = stdout;

    if (output_file_name) {
        out_file = fopen(output_file_name, "wb");
    }

    if (!out_file)
    {
        fprintf(stderr, "ERROR: Unable to open: %s\n", output_file_name);
        error_flag = true;
    }

    if (!error_flag)
    {
        // Printing the header to the output file
        fprintf(out_file, "#N\ttimestamp\tqshort\tqlong\tchannel\n");

        intmax_t counter = 0;

        // Here be where the data is actually read
        while (!feof(in_file) && !ferror(in_file)) {
            // First we create a single event that will hold what is read from the file
            struct event_PSD event;

            // The next event is then read from the input file
            const size_t result = fread(&event, sizeof(struct event_PSD), 1, in_file);

            if (verbosity > 1) {
                fprintf(stderr, "[%zu] Read result: %zu\n", counter, result);
            }

            // If exactly one event was read then we can expect that the reading is good
            if (result == 1) {
                // Pulse example
                // =============
                //
                // baseline -> ooo ------------------------------ oooooooooooo
                //                o                      ooooooooo
                //                o               ooooooo
                //                o          ooooo
                //                 o      ooo
                //                 o    oo
                //                 o  oo
                //                  oo
                // qshort ->    |-------|
                // qlong  ->    |---------------------------------------|
                //               ^^^ Integrations domains ^^^
                //                 
                // Printing some of the struct members:
                // counter: just a counter of all the read events
                fprintf(out_file, "%" PRIuMAX "\t", counter);
                // timestamp: a 64 bit unsigned integer holding the timestamp
                //            information, this is not calibrated.
                fprintf(out_file, "%" PRIu64 "\t", event.timestamp);
                // qshort: a 16 bit unsigned integer holding the short integral
                //         i.e. the integral over the first part of the pulses
                fprintf(out_file, "%" PRIu16 "\t", event.qshort);
                // qlong:  a 16 bit unsigned integer holding the long integral
                //         i.e. the integral over the whole pulse and thus the
                //         total energy of the pulse
                fprintf(out_file, "%" PRIu16 "\t", event.qlong);
                // channel: the number of the digitizer channel that acquired
                //          this event
                fprintf(out_file, "%" PRIu8 "\n", event.channel);
                // baseline: the pulse baseline
                //fprintf(out_file, "%" PRIu16 "\t", event.baseline);
                // pur: unused byte of the event struct
                //fprintf(out_file, "%" PRIu8 "\n", event.pur);

                counter += 1;
            } else {
                if (verbosity > 0) {
                    fprintf(stderr, "Reached the end of the file\n");
                }
            }
        }

        // Checking the files status
        if (ferror(in_file)) {
            fprintf(stderr, "ERROR: Error reading: %s\n", input_file_name);
            error_flag = true;
        }
        if (ferror(out_file)) {
            fprintf(stderr, "ERROR: Error writing to: %s\n", output_file_name);
            error_flag = true;
        }
    }

    // Closing files and freeing memory to keep things clean
    if (in_file) {
        fclose(in_file);
    }
    if (out_file) {
        fclose(out_file);
    }
    if (output_file_name) {
        free(output_file_name);
    }
    if (input_file_name) {
        free(input_file_name);
    }

    if (error_flag) {
        return EXIT_FAILURE;
    } else {
        return EXIT_SUCCESS;
    }
}

void print_usage(const char *name) {
    printf("Usage: %s [options] <file_name>\n", name);
    printf("\n");
    printf("Reads and prints an ABCD events file converting it to ASCII\n");
    printf("\n");
    printf("Optional arguments:\n");
    printf("\t-h: Display this message\n");
    printf("\t-o <output_file>: Write to a file instead of the stdout\n");
    printf("\t-v: Set verbose execution\n");
    printf("\t-V: Set more verbose execution\n");

    return;
}

int parse_command_line(int argc, char *argv[], unsigned int *verbosity, char **output_file_name, char **input_file_name) {
    int c = 0;
    while ((c = getopt(argc, argv, "ho:vV")) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'o': {
                const size_t name_length = strlen(optarg);
                (*output_file_name) = calloc(name_length + 1, sizeof(char));
                memcpy(*output_file_name, optarg, name_length);
                }
                break;
            case 'v':
                (*verbosity) = 1;
                break;
            case 'V':
                (*verbosity) = 2;
                break;
            default:
                fprintf(stderr, "ERROR: Unknown command: %c", c);
                break;
        }
    }

    if (optind >= argc)
    {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const size_t name_length = strlen(argv[optind]);
    (*input_file_name) = calloc(name_length + 1, sizeof(char));
    memcpy(*input_file_name, argv[optind], name_length);

    if ((*verbosity) > 0) {
        fprintf(stderr, "Input file: %s\n", *input_file_name);
        if (output_file_name) {
            fprintf(stderr, "Output file: %s\n", *output_file_name);
        } else {
            fprintf(stderr, "Output file: stdout\n");
        }
        fprintf(stderr, "Verbosity: %u\n", *verbosity);
    }

    return EXIT_SUCCESS;
}
