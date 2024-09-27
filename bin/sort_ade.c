/*
 * (C) Copyright 2023, 2024 European Union, Cristiano Lino Fontana
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
 *
 * Compile with:
 *         gcc -w -Wall -pedantic -O3 sort_ade.c -o sort_ade
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <getopt.h>

#include <time.h>

#include "events.h"

#define BUFFER_SIZE_UNIT 1000000
#define GiB (1024.0 * 1024.0 * 1024.0)

// Time between prints of the current index, in seconds
#define TIC_TIME 0.6
#define SECONDS_PER_NANOSECOND 1e-9

void print_usage(const char *name);

double time_difference(struct timespec t1, struct timespec t2) {
    const double seconds = t1.tv_sec - t2.tv_sec;
    const double nanoseconds = t1.tv_nsec - t2.tv_nsec;
    const double delta = seconds + nanoseconds * SECONDS_PER_NANOSECOND;

    return delta;
}

int earlier_than(struct event_PSD A, struct event_PSD B) {
    return (A.timestamp < B.timestamp);
}

struct event_PSD read_event_buffer(struct event_PSD *buffer, uintmax_t index) {
    return buffer[index];
}

void write_event_buffer(struct event_PSD *buffer, uintmax_t index, struct event_PSD event) {
    buffer[index] = event;
}

struct event_PSD read_event_file(FILE *file, uintmax_t index) {
    struct event_PSD event;

    // Move the file pointer to the position of index
    fseek(file, index * sizeof(struct event_PSD), SEEK_SET);

    fread(&event, sizeof(struct event_PSD), 1, file);

    return event;
}

void write_event_file(FILE *file, uintmax_t index, struct event_PSD event) {
    // Move the file pointer to the position of index
    fseek(file, index * sizeof(struct event_PSD), SEEK_SET);

    fwrite(&event, sizeof(struct event_PSD), 1, file);

    fflush(file);
}

#define swap_events(type, source, index_A, index_B) { \
    const struct event_PSD event_A = read_event_ ## type ((source), (index_A)); \
    const struct event_PSD event_B = read_event_ ## type ((source), (index_B)); \
    write_event_ ## type ((source), (index_B), (event_A)); \
    write_event_ ## type ((source), (index_A), (event_B)); \
}


#define insertion_sort(type, source, number_of_events, verbosity) { \
    if ((verbosity) > 0) { \
        printf("Sorting " #type " at pointer: %#010" PRIxMAX "; with number_of_events: %" PRIuMAX " (%.2f M events);\n", (uintmax_t)(source), (number_of_events), (float)(number_of_events) / BUFFER_SIZE_UNIT); \
    } \
    if ((verbosity) > 1) { \
        printf("\n"); \
    } \
    struct timespec last_tic; \
    uintmax_t counter_swaps = 0; \
    clock_gettime(CLOCK_REALTIME, &last_tic); \
    for (uintmax_t index_i = 1; index_i < (number_of_events); index_i++) \
    { \
        if ((verbosity) > 1) { \
            struct timespec now; \
            clock_gettime(CLOCK_REALTIME, &now); \
            const double delta = time_difference(now, last_tic); \
            if (delta > TIC_TIME) { \
                printf("\033[A\033[2K\r"); \
                printf("At index: %" PRIuMAX "/%" PRIuMAX " (%.2f%%);\n", index_i, (number_of_events), index_i / (double)(number_of_events) * 100); \
                clock_gettime(CLOCK_REALTIME, &last_tic); \
            } \
        } \
        uintmax_t index_j = index_i; \
        const struct event_PSD current = read_event_ ## type ((source), index_j); \
        while (index_j > 0) \
        { \
            struct event_PSD previous = read_event_ ## type ((source), index_j - 1); \
            if (earlier_than(current, previous)) \
            { \
                if ((verbosity) > 2) { \
                    printf("Swapping indexes: %" PRIuMAX " and %" PRIuMAX ";\n", index_j, index_j - 1); \
                } \
                swap_events(type, (source), index_j, index_j - 1); \
                counter_swaps += 1; \
                index_j -= 1; \
            } else {\
                break; \
            } \
        } \
    } \
    if ((verbosity) > 0) { \
        printf("swaps: %" PRIuMAX ";\n", counter_swaps); \
    } \
}

int main(int argc, char *argv[])
{
    unsigned int verbosity = 0;
    uintmax_t buffer_size = BUFFER_SIZE_UNIT;
    bool disable_sort_on_disk = false;

    int c = 0;
    while ((c = getopt(argc, argv, "hvb:d")) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'b':
                buffer_size = strtoul(optarg, NULL, 0) * BUFFER_SIZE_UNIT;
                break;
            case 'v':
                verbosity += 1;
                break;
            case 'd':
                disable_sort_on_disk = true;
                break;
            default:
                printf("Unknown command: %c", c);
                break;
        }
    }

    const int number_of_files = argc - optind;

    if (number_of_files <= 0)
    {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    if (verbosity > 0) {
        printf("Found file names: %d\n", number_of_files);
        printf("File name(s):\n");
        for (int i = 0; i < number_of_files; i++)
        {
            printf("\t%s\n", argv[i + optind]);
        }
        printf("Buffer size: %" PRIuMAX " M event (%.3f GiB)\n", buffer_size / BUFFER_SIZE_UNIT, buffer_size * sizeof(struct event_PSD) / GiB);
        printf("Verbosity: %u\n", verbosity);
        if (disable_sort_on_disk) {
            printf("Sort on disk is disabled!\n");
        }
    }

    // Allocate memory for the buffer
    struct event_PSD *buffer = malloc(buffer_size * sizeof(struct event_PSD));
    if (!buffer)
    {
        fprintf(stderr, "ERROR: could not allocate memory for the buffer\n");

        return EXIT_FAILURE;
    }

    for (int index_files = optind; index_files < argc; index_files += 1)
    {
        uintmax_t number_of_events = 0;

        const char *file_name = argv[index_files];

        if (verbosity > 0) {
            printf("Opening file: %s\n", file_name);
        }

        // Open the file in read and write mode
        FILE *file = fopen(file_name, "r+b");

        if (!file) {
            fprintf(stderr, "ERROR: Unable to open file for the first time\n");
        } else {
            // Move the file pointer to the end of the file
            fseek(file, 0, SEEK_END);

            const uintmax_t file_size = ftell(file);

            if (file_size % sizeof(struct event_PSD) != 0) {
                fprintf(stderr, "WARNING: file size is not a multiple of %zu bytes. Ignoring the last %zu bytes.\n", sizeof(struct event_PSD), file_size % sizeof(struct event_PSD));
            }

            number_of_events = file_size / sizeof(struct event_PSD);

            if (verbosity > 0) {
                printf("Number of events in the file: %" PRIuMAX " (%.2f M events = %.3f GiB)\n", number_of_events, (double)number_of_events / BUFFER_SIZE_UNIT, number_of_events * sizeof(struct event_PSD) / GiB);
            }

            // Move the file pointer to the begin of the file
            fseek(file, 0, SEEK_SET);

            if (verbosity > 0) {
                printf("Sorting file in %" PRIuMAX " full buffers + a buffer of %" PRIuMAX " events (%.2f M events = %.3f GiB)\n", number_of_events / buffer_size, number_of_events % buffer_size, (double)(number_of_events % buffer_size) / BUFFER_SIZE_UNIT, (number_of_events % buffer_size) * sizeof(struct event_PSD) / GiB);
            }

            // Read the file one buffer at a time
            uintmax_t counter_buffers = 0;
            uintmax_t counter_events = 0;
            while ((counter_events = fread(buffer, sizeof(struct event_PSD), buffer_size, file)) > 0)
            {
                if (verbosity > 0) {
                    printf("Sorting buffer number: %" PRIuMAX "; size: %" PRIuMAX " events\n", counter_buffers, counter_events);
                }

                insertion_sort(buffer, buffer, counter_events, verbosity);

                // Move the file pointer back to the start of the buffer
                fseek(file, -counter_events * sizeof(struct event_PSD), SEEK_CUR);

                // Write the sorted buffer back to the file
                fwrite(buffer, sizeof(struct event_PSD), counter_events, file);

                fflush(file);

                counter_buffers += 1;
            }

            if (!disable_sort_on_disk) {
                // Move the file pointer to the begin of the file
                fseek(file, 0, SEEK_SET);

                if (verbosity > 0) {
                    printf("Sorting file in-place...\n");
                }

                insertion_sort(file, file, number_of_events, verbosity);
            }


            if (verbosity > 0) {
                printf("Closing file...\n");
            }

            fclose(file);
        }
    }

    if (buffer) {
        free(buffer);
    }

    return EXIT_SUCCESS;
}

void print_usage(const char *name) {
    printf("Usage: %s [options] <file_name> [<file_name> ...]\n", name);
    printf("\n");
    printf("Sorts ade files based on the events' timestamps.\n");
    printf("The sorting happens, in a single thread, in two stages:\n");
    printf("1. The file is read in buffers that are sorted in memory then written back to the file.\n");
    printf("2. If enabled, the file is sorted in-place on the whole file itself on the disk drive.\n");
    printf("\n");
    printf("WARNING: If the sorting on the file is not enabled then the sorting is only partial!\n");
    printf("\n");
    printf("WARNING: The sorting is done on the file itself, make sure that you have a back up!\n");
    printf("\n");
    printf("WARNING: Expect a much slower second step than the first\n");
    printf("\n");
    printf("Optional arguments:\n");
    printf("\t-h: Display this message\n");
    printf("\t-d: Disables the sorting in-place on the file\n");
    printf("\t-b <buffer_size>: Buffer size for the pre-sorting, in multiples of 1 million events, default: 1\n");
    printf("\t                  The size of one PSD event is %u B so 1 million events = %u MiB.\n", (unsigned int)sizeof(struct event_PSD), (unsigned int)sizeof(struct event_PSD) * BUFFER_SIZE_UNIT / 1024);
    printf("\t-v: Set verbose execution, using it multiple times increases the verbosity level.\n");
    printf("\t    With a verbosity of 2 it prints the progress of the sorting.\n");

    return;
}

