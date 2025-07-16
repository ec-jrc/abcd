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
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <map>

#include <cstdint>
#include <cinttypes>

extern "C"
{
#include <getopt.h>

#include "events.h"
}

#define BUFFER_SIZE_UNIT 1000000

void print_usage(const char *name);

int main(int argc, char *argv[])
{
    unsigned int verbosity = 0;
    size_t buffer_size = 10 * BUFFER_SIZE_UNIT;

    int c = 0;
    while ((c = getopt(argc, argv, "hvb:t:T:j:s")) != -1)
    {
        switch (c)
        {
        case 'h':
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        case 'b':
            buffer_size = std::stod(optarg) * BUFFER_SIZE_UNIT;
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

    if (verbosity > 0)
    {
        std::cout << "Input file name: " << input_file_name << std::endl;
        std::cout << "Buffer size: " << buffer_size / BUFFER_SIZE_UNIT << " M events" << std::endl;
        std::cout << "Verbosity: " << verbosity << std::endl;
    }

    std::ifstream input_file(input_file_name, std::ios::binary);

    if (!input_file)
    {
        std::cerr << "ERROR: Unable to open input file" << std::endl;

        return EXIT_FAILURE;
    }

    if (verbosity > 1)
    {
        std::cout << "Opened input file: " << input_file_name << std::endl;
    }

    size_t counter_buffers = 0;
    size_t counter_total = 0;

    std::map<uint8_t, size_t> counters_events;
    std::vector<struct event_PSD> buffer(buffer_size);

    while (!input_file.eof())
    {
        input_file.read(reinterpret_cast<char *>(buffer.data()), buffer_size * sizeof(struct event_PSD));

        const size_t num_read = input_file.gcount() / sizeof(struct event_PSD);

        for (size_t index_local = 0; index_local < num_read; index_local++)
        {
            const uint8_t channel = buffer[index_local].channel;

            if (verbosity > 3)
            {
                std::cout << "i: " << counter_total << "; current channel: " << static_cast<unsigned int>(channel) << std::endl;
            }

            counters_events[channel] += 1;

            counter_total += 1;
        }

        counter_buffers += 1;
    }

    if (verbosity > 0)
    {
        std::cout << "Total number of events: " << counter_total << std::endl;
        std::cout << "Number of buffers read: " << counter_buffers << std::endl;
    }

    for (auto const& pair: counters_events) {
        std::cout << static_cast<unsigned int>(pair.first) << " " << pair.second << std::endl;
    }

    input_file.close();
    return 0;
}

void print_usage(const char *name)
{
    std::cout << "Usage: " << name << " [options] <file_name>" << std::endl;
    std::cout << std::endl;
    std::cout << "Reads an ABCD events file and counts the numer of events for each found channel." << std::endl;
    std::cout << std::endl;
    std::cout << "Optional arguments:" << std::endl;
    std::cout << "\t-h: Display this message" << std::endl;
    std::cout << "\t-b <buffer_size>: Buffer size for the events reading, in multiples of 1 million events, default: 10" << std::endl;
    std::cout << "\t-v: Set verbose execution, using it multiple times increases the verbosity" << std::endl;

    return;
}