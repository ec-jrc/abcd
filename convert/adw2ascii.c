/*
 * (C) Copyright 2021, European Union, Cristiano Lino Fontana
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
        intmax_t counter = 0;

        // Here be where the data is actually read
        while (!feof(in_file) && !ferror(in_file)) {
            // First we read the header of the waveform. It is a 14 bytes word containing:
            // timestamp: a 64 bit unsigned integer holding the timestamp
            //            information, this is not calibrated.
            uint64_t timestamp;
            // channel: the number of the digitizer channel that acquired
            //          this event
            uint8_t channel;
            // samples_number: the number of the samples in the waveform
            uint32_t samples_number;
            // gates_number: the number of additional waveforms
            uint8_t gates_number;

            // We read the file content storing it in each variable,
            // checking the reading results.
            size_t result = 0;

            result += fread(&timestamp, sizeof(timestamp), 1, in_file) * sizeof(timestamp);
            result += fread(&channel, sizeof(channel), 1, in_file) * sizeof(channel);
            result += fread(&samples_number, sizeof(samples_number), 1, in_file) * sizeof(samples_number);
            result += fread(&gates_number, sizeof(gates_number), 1, in_file) * sizeof(gates_number);

            if (verbosity > 1) {
                fprintf(stderr, "[%zu] Read result: %zu\n", counter, result);
            }

            // If 14 bytes were read then we can expect that the reading is good
            if (result == 14) {
                // We need to allocate an array in which we can store the samples...
                uint16_t *samples = malloc(samples_number * sizeof(uint16_t));
                // ...and an array in which we can store the additional waveforms
                uint16_t *additional_samples = malloc(samples_number * sizeof(uint8_t));
                // In this simple example we are assuming that the allocations were successful

                // We can now read the samples of the waveform
                result = fread(samples, sizeof(uint16_t), samples_number, in_file);
                // In this simple example we are assuming that the read was successful

                // We write the header of the waveform to the output file
                fprintf(out_file, "# index: %" PRIuMAX ", timestamp: %" PRIu64 ", channel: %" PRIu8 "\n", counter, timestamp, channel);

                // We write the samples in one line
                for (uint32_t i = 0; i < samples_number; i++) {
                    fprintf(out_file, "%" PRIu16 "\t", samples[i]);

                }
                fprintf(out_file, "\n");

                // We read anf write all the additional waveforms
                for (uint32_t j = 0; j < gates_number; j++) {
                    result = fread(additional_samples, sizeof(uint8_t), samples_number, in_file);
                    // In this simple example we are assuming that the read was successful

                    // The additional waveforms are written on new lines
                    for (uint32_t i = 0; i < samples_number; i++) {
                        fprintf(out_file, "%" PRIu8 "\t", additional_samples[i]);

                    }
                    fprintf(out_file, "\n");
                }

                // We need to free the allocated memory
                free(samples);
                free(additional_samples);

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
    printf("Reads and prints an ABCD waveforms file converting it to ASCII\n");
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
