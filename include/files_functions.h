/*
 * (C) Copyright 2023, European Union, Cristiano Lino Fontana
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

#ifndef __FILES_FUNCTIONS_C__
#define __FILES_FUNCTIONS_C__ 1

#include <stdio.h>
// For malloc
#include <stdlib.h>
// For memcpy
#include <string.h>
#include <stdbool.h>

#include "defaults.h"

// Read a message from an ABCD raw file, parameters:
// - file: the pointer to the file (INPUT);
// - topic: the address of a pointer to a string (OUTPUT),
//          the memory will be allocated in the function, it needs to be freed;
// - buffer: the address of a pointer to a buffer of data (OUTPUT);
//           the memory will be allocated in the function, it needs to be freed;
// - size: the address of the size of the buffer (OUTPUT);
// - extract_topic: if requested a topic will be extracted;
// - verbosity: a flag to activate debug output (INPUT).
extern inline int read_byte_message_from_raw(FILE *input_file, char **topic, void **buffer, size_t *size, bool extract_topic, unsigned int verbosity)
{
    // Set size to 0 to notify that no message was received or an error occurred
    *size = 0;

    if (!input_file)
    {
        printf("ERROR: File not opened\n");

        return EXIT_FAILURE;

    } else if (feof(input_file)) {
        printf("ERROR: Reached end of file\n");

        return EXIT_FAILURE;

    } else if (ferror(input_file)) {
        printf("ERROR: File error\n");

        return EXIT_FAILURE;

    } else {
        char topic_buffer[defaults_all_topic_buffer_size];
        size_t partial_counter = 0;

        memset(topic_buffer, '\0', defaults_all_topic_buffer_size);

        int c;
        while ((c = fgetc(input_file)) != EOF)
        {
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
                    printf("Topic: %s\n", topic_buffer);
                }

                // Looking for the size of the message by searching for the last '_'
                char *size_pointer = strrchr(topic_buffer,'_');
                size_t data_size = 0;
                if (size_pointer == NULL)
                {
                    printf("ERROR: Unable to find message size, topic: %s\n", topic_buffer);
                }
                else
                {
                    data_size = atoi(size_pointer + 2);
                }

                const size_t topic_size = strlen(topic_buffer);

                if (verbosity > 0)
                {
                    printf("Topic size: %zu; Data size: %zu\n", topic_size, data_size);
                }

                void *data_buffer = malloc(sizeof(char) * data_size);

                const size_t bytes_read = fread(data_buffer, sizeof(char), data_size, input_file);

                if (verbosity > 1)
                {
                    printf("read: %zu, requested: %zu\n", bytes_read, data_size * sizeof(char));
                }

                if (bytes_read != (sizeof(char) * data_size))
                {
                    printf("ERROR: Unable to read all the requested bytes: read: %zu, requested: %zu\n", bytes_read, data_size * sizeof(char));
                }

                if (extract_topic) {
                    // Allocates the topic buffer
                    *topic = (char *)calloc(topic_size + 1, sizeof(char));

                    if (!(*topic))
                    {
                        printf("ERROR: Unable to allocate the topic buffer\n");

                        free(data_buffer);

                        return EXIT_FAILURE;
                    }

                    memcpy(*topic, topic_buffer, topic_size);

                    // Remeber to terminate the string!!!
                    (*topic)[topic_size] = '\0';


                    // Allocates the data buffer
                    *buffer = malloc(data_size * sizeof(char));

                    if (!(*buffer))
                    {
                        printf("ERROR: Unable to allocate the topic buffer\n");

                        free(data_buffer);
                        free(*topic);

                        return EXIT_FAILURE;
                    }

                    memcpy(*buffer, data_buffer, data_size);

                    *size = data_size;
                } else {
                    *topic = NULL;

                    // Allocates the buffer
                    *buffer = malloc((topic_size + 1 + data_size) * sizeof(char));

                    if (!(*buffer))
                    {
                        printf("ERROR: Unable to allocate the buffer\n");

                        free(data_buffer);

                        return EXIT_FAILURE;
                    }

                    memcpy(*buffer, topic_buffer, topic_size);

                    // Remember to add the topic separator!
                    ((char*)*buffer)[topic_size] = ' ';

                    memcpy((void*)((char*)*buffer + (topic_size + 1)), data_buffer, data_size);

                    *size = topic_size + 1 + data_size;
                }

                if (data_buffer) {
                    free(data_buffer);
                }

                return EXIT_SUCCESS;
            }
        }
    }

    return EXIT_SUCCESS;
}

#endif
