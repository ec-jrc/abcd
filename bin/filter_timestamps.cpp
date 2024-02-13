/*
 * (C) Copyright 2024 European Union, Cristiano Lino Fontana
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
 *      g++ -w -Wall -pedantic -O3 filter_timestamps.cpp -o filter_timestamps
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip> // For std::setw()
#include <string>
#include <sstream>
//#include <algorithm>

#include <cstdlib>
#include <cstdint>
#include <cinttypes>

extern "C" {
#include <getopt.h>

#include "events.h"
}

#define BUFFER_SIZE_UNIT 1000000

#define DEFAULT_TIMESTAMP_MINIMUM 0
#define DEFAULT_TIMESTAMP_MAXIMUM UINT64_MAX
#define DEFAULT_TIMESTAMP_JUMP 0

void print_usage(const char *name);

bool ends_with(std::string input, std::string ending);
std::string remove_ade_extension(std::string input);
std::string create_output_file_name(std::string input_file_name, size_t counter);

int main(int argc, char* argv[])
{
    unsigned int verbosity = 0;
    size_t buffer_size = BUFFER_SIZE_UNIT;

    uint64_t timestamp_minimum = DEFAULT_TIMESTAMP_MINIMUM;
    uint64_t timestamp_maximum = DEFAULT_TIMESTAMP_MAXIMUM;
    uint64_t timestamp_jump = DEFAULT_TIMESTAMP_JUMP;

    int c = 0;
    while ((c = getopt(argc, argv, "hvb:t:T:j:")) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'b':
                buffer_size = std::stod(optarg) * BUFFER_SIZE_UNIT;
                break;
            case 't':
                timestamp_minimum = std::stod(optarg);
                break;
            case 'T':
                timestamp_maximum = std::stod(optarg);
                break;
            case 'j':
                timestamp_jump = std::stod(optarg);
                break;
            case 'v':
                verbosity += 1;
                break;
            default:
                printf("Unknown command: %c", c);
                break;
        }
    }

    if (argc <= optind)
    {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    const std::string input_file_name(argv[optind]);

    if (verbosity > 0) {
        std::cout << "Input file name: " << input_file_name << std::endl;
        std::cout << "Output file basename: " << remove_ade_extension(input_file_name) << std::endl;
        std::cout << "Buffer size: " << buffer_size / BUFFER_SIZE_UNIT << " M event" << std::endl;
        std::cout << "Timestamp minimum: " << timestamp_minimum << " (0x" << std::hex << timestamp_minimum << std::dec << ")" << std::endl;
        std::cout << "Timestamp maximum: " << timestamp_maximum << " (0x" << std::hex << timestamp_maximum << std::dec << ")" << std::endl;
        std::cout << "Timestamp jump: " << timestamp_jump << " (0x" << std::hex << timestamp_jump << std::dec << ")" << std::endl;
        std::cout << "Verbosity: " << verbosity << std::endl;
    }

    std::ifstream input_file(input_file_name, std::ios::binary);

    if (!input_file) {
        std::cerr << "ERROR: Unable to open input file" << std::endl;

        return EXIT_FAILURE;
    }

    if (verbosity > 1) {
        std::cout << "Opened input file: " << input_file_name << std::endl;
    }

    size_t counter_output_files = 0;
    const std::string output_file_name = create_output_file_name(input_file_name, counter_output_files);

    std::ofstream output_file;
    output_file.open(output_file_name, std::ios::binary);

    if (!output_file) {
        std::cerr << "ERROR: Unable to open output file number: " << counter_output_files << std::endl;

        return EXIT_FAILURE;
    }

    counter_output_files += 1;

    if (verbosity > 1) {
        std::cout << "Opened output file: " << output_file_name << std::endl;
    }

    size_t counter_buffers = 0;
    size_t counter_discarded = 0;
    size_t counter_total = 0;

    std::vector<struct event_PSD> buffer(buffer_size);

    uint64_t timestamp_previous = 0;

    while (!input_file.eof()) {
        input_file.read(reinterpret_cast<char*>(buffer.data()), buffer_size * sizeof(struct event_PSD));

        const size_t num_read = input_file.gcount() / sizeof(struct event_PSD);

        for (size_t index_local = 0; index_local < num_read; index_local++) {
            const uint64_t timestamp_current = buffer[index_local].timestamp;

            if (verbosity > 3) {
                std::cout << "i: " << counter_total << "; current timestamp: 0x" << std::hex << timestamp_current << std::dec << std::endl;
            }

            // Discard timestamps outside the desired range, before checking for
            // jumps, because we assume that these values are spurious
            if (timestamp_current < timestamp_minimum || timestamp_maximum < timestamp_current) {
                if (verbosity > 2) {
                    std::cout << "i: " << counter_total << "; discarding timestamp: 0x" << std::hex << timestamp_current << std::dec << std::endl;
                }

                counter_discarded += 1;
            } else {
                // If there is a jump backward in time above a threshold then change output file
                // WARNING: These are unsigned integers, so better not to use
                //          subtractions that can cause overflows.
                if ((timestamp_current + timestamp_jump) < timestamp_previous) {
                    if (verbosity > 1) {
                        std::cout << "i: " << counter_total << "; detected jump: 0x" << std::hex << timestamp_previous << std::dec << " to 0x" << std::hex << timestamp_current << std::dec << std::endl;
                        std::cout << "i: " << counter_total << "; discarded so far: " << counter_discarded << " (" << static_cast<double>(counter_discarded) / counter_total * 100.0 << "%)" << std::endl;
                    }

                    if (output_file.is_open()) {
                        output_file.close();
                    }

                    const std::string output_file_name = create_output_file_name(input_file_name, counter_output_files);

                    output_file.open(output_file_name, std::ios::binary);

                    if (!output_file) {
                        std::cerr << "ERROR: Unable to open output file number: " << counter_output_files << std::endl;

                        return EXIT_FAILURE;
                    }

                    counter_output_files += 1;

                    if (verbosity > 1) {
                        std::cout << "Opened output file: " << output_file_name << std::endl;
                    }

                }

                output_file.write(reinterpret_cast<char*>(&buffer[index_local]), sizeof(struct event_PSD));

                timestamp_previous = timestamp_current;
            }

            counter_total += 1;
        }

        counter_buffers += 1;
    }

    if (verbosity > 0) {
        std::cout << "Total number of events: " << counter_total << "; Discarded events: " << counter_discarded << " (" << static_cast<double>(counter_discarded) / counter_total * 100.0 << "%)" << std::endl;
        std::cout << "Number of buffers read: " << counter_buffers << std::endl;
        std::cout << "Number of output files: " << counter_output_files << std::endl;
    }

    if (output_file.is_open()) {
        output_file.close();
    }

    input_file.close();
    return 0;
}

void print_usage(const char *name) {
    std::cout << "Usage: " << name << " [options] <file_name>" << std::endl;
    std::cout << std::endl;
    std::cout << "Reads an ABCD events file and discards events with a timestamp outsize the selected values." << std::endl;
    std::cout << "If it detects a jump backwards in time, it opens a new output file writing the new events to the new output file." << std::endl;
    std::cout << "Small jumps backward in time are physiological due to the channels buffering, so there is a threshold for the minimum jump to consider." << std::endl;
    std::cout << std::endl;
    std::cout << "Optional arguments:" << std::endl;
    std::cout << "\t-h: Display this message" << std::endl;
    std::cout << "\t-b <buffer_size>: Buffer size for the events reading, in multiples of 1 million events, default: 1" << std::endl;
    std::cout << "\t-t: Minimum value of accepted timestamps, default: 0, reasonable value: 1e11" << std::endl;
    std::cout << "\t-T: Maximum value of accepted timestamps, default: UINT64_MAX, reasonable value: 1e18" << std::endl;
    std::cout << "\t-j: Maximum value of accepted jump backward in time, default: 0, reasonable value: 1e14" << std::endl;
    std::cout << "\t-v: Set verbose execution, using it multiple times increases the verbosity" << std::endl;

    return;
}

bool ends_with(std::string input, std::string ending) {
    if (input.length() >= ending.length()) {
        return (0 == input.compare(input.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

std::string remove_ade_extension(std::string input) {
    if (ends_with(input, ".ade")) {
        return input.substr(0, input.size() - 4);
    } else {
        return input;
    }
}

std::string create_output_file_name(std::string input_file_name, size_t counter) {
    const std::string output_basename = remove_ade_extension(input_file_name);

    std::stringstream s;

    s << std::setw(4) << std::setfill('0') << counter;

    return output_basename + "_chunk" + s.str() + ".ade";
}
