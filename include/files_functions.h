/*
 * (C) Copyright 2023, 2025, European Union, Cristiano Lino Fontana
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
#include "utilities_functions.h"

#define ADDRESS_PREFIX_FILE "file://"
#define ADDRESS_BUFFER_SIZE_SEPARATOR ":"

enum input_sources_t
{
    UNKNOWN_INPUT,
    SOCKET_INPUT,
    RAW_FILE_INPUT,
    EVENTS_FILE_INPUT,
    WAVEFORMS_FILE_INPUT,
};

/// @brief Returns the input source type type to identify the events source.
/// Addresses that start with 'file://' can be interpreted as ABCD data files:
/// 1. Raw files should have an address of the form 'file://<path_to_file>.adr'
/// 2. Events files should have an address of the form 'file://<path_to_file>.ade[:<buffer_size>]'
///    The <buffer_size> is optional
/// 3. Waveforms files should have an address of the form 'file://<path_to_file>.adw[:<buffer_size>]'
///    The <buffer_size> is optional
/// Otherwise the address is interpreted as a socket address
///
/// @param address A string containing the address
/// @return An enum input_sources_t
extern inline enum input_sources_t get_input_source_type(const char *address)
{
    if (strstr(address, ADDRESS_PREFIX_FILE) != address)
    {
        return SOCKET_INPUT;
    }
    else
    {
        // If it does not end with '.adr' then it may end with a size or with .ade or .adw
        if (ends_with(address, ".adr"))
        {
            return RAW_FILE_INPUT;
        }
        else if (ends_with(address, ".ade"))
        {
            return EVENTS_FILE_INPUT;
        }
        else if (ends_with(address, ".adw"))
        {
            return WAVEFORMS_FILE_INPUT;
        }
        else
        {
            // There might be a buffer size
            if (rstrstr(address, ".ade" ADDRESS_BUFFER_SIZE_SEPARATOR))
            {
                return EVENTS_FILE_INPUT;
            }
            else if (rstrstr(address, ".adw" ADDRESS_BUFFER_SIZE_SEPARATOR))
            {
                return WAVEFORMS_FILE_INPUT;
            }
            else
            {
                return UNKNOWN_INPUT;
            }
        }
    }
}

/// @brief Extract from a file-type address the file_name and the associated buffer_size (if specified)
/// @param[in] address Addresses that start with 'file://' can be interpreted as ABCD data files:
/// 1. Raw files should have an address of the form 'file://<path_to_file>.adr'
/// 2. Events files should have an address of the form 'file://<path_to_file>.ade[:<buffer_size>]'
///    The <buffer_size> is optional
/// 3. Waveforms files should have an address of the form 'file://<path_to_file>.adw[:<buffer_size>]'
///    The <buffer_size> is optional
/// Otherwise the address is interpreted as a socket address
/// @param[out] file_name The extracted file name, the memory will be allocated in the function if no error occurred, it needs to be freed
/// @param[out] buffer_size The buffer size as specified in the address, if not specified it returns 0 (no multiplication to sizeof(event_PSD) is performed)
/// @return EXIT_SUCCESS on success, EXIT_FAILURE on failure
extern inline int get_filename_buffersize(const char *address, char **file_name, size_t *buffer_size)
{
    const enum input_sources_t type = get_input_source_type(address);

    (*file_name) = (char *)calloc(strlen(address) + 1, sizeof(char));

    if (!(*file_name))
    {
        printf("ERROR: Unable to allocate file_name\n");
        return EXIT_FAILURE;
    }

    printf("Empty filename: '%s' %d\n", (*file_name), type);

    if (type == RAW_FILE_INPUT)
    {
        // Raw files should have an address of the form 'file://<path_to_file>.adr'
        strcpy((*file_name), address + strlen(ADDRESS_PREFIX_FILE));

        printf("Raw filename: '%s'\n", (*file_name));

        (*buffer_size) = 0;

        return EXIT_SUCCESS;
    }
    else if (type == EVENTS_FILE_INPUT || type == WAVEFORMS_FILE_INPUT)
    {
        // Events files should have an address of the form 'file://<path_to_file>.ade[:<buffer_size>]'
        // Waveforms files should have an address of the form 'file://<path_to_file>.adw[:<buffer_size>]'
        const char *extension = (type == EVENTS_FILE_INPUT) ? ".ade" ADDRESS_BUFFER_SIZE_SEPARATOR : ".adw" ADDRESS_BUFFER_SIZE_SEPARATOR;
        char *str_buffer_size = rstrstr(address, extension);
        str_buffer_size = (str_buffer_size) ? str_buffer_size + strlen(extension) : NULL;

        if (str_buffer_size)
        {
            strncpy((*file_name), address + strlen(ADDRESS_PREFIX_FILE), strlen(address) - strlen(ADDRESS_PREFIX_FILE) - 1 - strlen(str_buffer_size));
            (*buffer_size) = abs(atol(str_buffer_size));
        }
        else
        {
            strcpy((*file_name), address + strlen(ADDRESS_PREFIX_FILE));
            (*buffer_size) = 0;
        }

        return EXIT_SUCCESS;
    }
    else
    {
        free(*file_name);
        (*file_name) = NULL;
        return EXIT_FAILURE;
    }
}

/// @brief Read a binary message from an ABCD events file, the message size should be given to the function as input and the topic will be generated accordingly
/// @param[in] input_file the pointer to the file
/// @param[out] topic the address of a pointer to a string, the memory will be allocated in the function, it needs to be freed
/// @param[out] buffer the address of a pointer to a buffer of data, the memory will be allocated in the function, it needs to be freed
/// @param[in] size the address of the size of the buffer, this should be specified in bytes not number of events
/// @param[in] extract_topic if false, the topic will be placed in `buffer` with the standard separator, otherwise topic will be placed in `topic` and `buffer` will not contain it
/// @param[in] verbosity a flag to activate debug output
/// @return EXIT_SUCCESS on success, EXIT_FAILURE on failure
extern inline int read_byte_message_from_ade(FILE *input_file, char **topic, void **buffer, size_t *size, bool extract_topic, unsigned int verbosity)
{
    if ((*size) % sizeof(struct event_PSD) != 0)
    {
        printf("ERROR: Size not a multiple of sizeof(struct event_PSD)\n");

        // Set size to 0 to notify that no message was received or an error occurred
        *size = 0;

        return EXIT_FAILURE;
    }
    else if (!input_file)
    {
        printf("ERROR: File not opened\n");

        *size = 0;

        return EXIT_FAILURE;
    }
    else if (feof(input_file))
    {
        printf("ERROR: Reached end of file\n");

        *size = 0;

        return EXIT_FAILURE;
    }
    else if (ferror(input_file))
    {
        printf("ERROR: File error\n");

        *size = 0;

        return EXIT_FAILURE;
    }
    else
    {
        const size_t data_size = (*size);

        char topic_buffer[defaults_all_topic_buffer_size];

        memset(topic_buffer, '\0', defaults_all_topic_buffer_size);

        snprintf(topic_buffer, defaults_all_topic_buffer_size, "data_abcd_events_v0_s%zu", data_size);

        const size_t topic_size = strlen(topic_buffer);

        void *data_buffer = malloc(sizeof(char) * data_size);

        const size_t bytes_read = fread(data_buffer, sizeof(char), data_size, input_file);

        if (extract_topic)
        {
            // Allocates the topic buffer
            *topic = (char *)calloc(topic_size + 1, sizeof(char));

            if (!(*topic))
            {
                printf("ERROR: Unable to allocate the topic buffer\n");

                return EXIT_FAILURE;
            }

            memcpy(*topic, topic_buffer, topic_size);

            // Remeber to terminate the string!!!
            (*topic)[topic_size] = '\0';

            if (verbosity > 0)
            {
                printf("Topic: %s\n", (*topic));
            }

            // Allocates the data buffer
            *buffer = malloc(bytes_read * sizeof(char));

            if (!(*buffer))
            {
                printf("ERROR: Unable to allocate the topic buffer\n");

                free(data_buffer);
                free(*topic);

                return EXIT_FAILURE;
            }

            memcpy(*buffer, data_buffer, bytes_read);

            *size = bytes_read;
        }
        else
        {
            *topic = NULL;

            // Allocates the buffer
            *buffer = malloc((topic_size + 1 + bytes_read) * sizeof(char));

            if (!(*buffer))
            {
                printf("ERROR: Unable to allocate the buffer\n");

                free(data_buffer);

                return EXIT_FAILURE;
            }

            memcpy(*buffer, topic_buffer, topic_size);

            // Remember to add the topic separator!
            ((char *)*buffer)[topic_size] = ' ';

            memcpy((void *)((char *)*buffer + (topic_size + 1)), data_buffer, bytes_read);

            *size = topic_size + 1 + bytes_read;
        }

        if (data_buffer)
        {
            free(data_buffer);
        }

        return EXIT_SUCCESS;
    }

    return EXIT_SUCCESS;
}

/// @brief Read a binary message from an ABCD raw file
/// @param[in] input_file the pointer to the file
/// @param[out] topic the address of a pointer to a string, the memory will be allocated in the function, it needs to be freed
/// @param[out] buffer the address of a pointer to a buffer of data, the memory will be allocated in the function, it needs to be freed
/// @param[out] size the address of the size of the buffer
/// @param[in] extract_topic if false, the topic will be placed in `buffer` with the standard separator, otherwise topic will be placed in `topic` and `buffer` will not contain it
/// @param[in] verbosity a flag to activate debug output
/// @return EXIT_SUCCESS on success, EXIT_FAILURE on failure
extern inline int read_byte_message_from_adr(FILE *input_file, char **topic, void **buffer, size_t *size, bool extract_topic, unsigned int verbosity)
{
    // Set size to 0 to notify that no message was received or an error occurred
    *size = 0;

    if (!input_file)
    {
        printf("ERROR: File not opened\n");

        return EXIT_FAILURE;
    }
    else if (feof(input_file))
    {
        printf("ERROR: Reached end of file\n");

        return EXIT_FAILURE;
    }
    else if (ferror(input_file))
    {
        printf("ERROR: File error\n");

        return EXIT_FAILURE;
    }
    else
    {
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
                char *size_pointer = strrchr(topic_buffer, '_');
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

                if (extract_topic)
                {
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
                }
                else
                {
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
                    ((char *)*buffer)[topic_size] = ' ';

                    memcpy((void *)((char *)*buffer + (topic_size + 1)), data_buffer, data_size);

                    *size = topic_size + 1 + data_size;
                }

                if (data_buffer)
                {
                    free(data_buffer);
                }

                return EXIT_SUCCESS;
            }
        }
    }

    return EXIT_SUCCESS;
}

#endif
