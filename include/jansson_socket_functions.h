// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#ifndef __JANSSON_SOCKET_FUNCTIONS_C__
#define __JANSSON_SOCKET_FUNCTIONS_C__ 1

// This macro is to use nanosleep even with compilation flag: -std=c99
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif
// This macro is to enable snprintf() in macOS
#ifndef _C99_SOURCE
#define _C99_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <jansson.h>
#include "socket_functions.h"

#define defaults_topic_buffer_size 1024

inline extern int send_json_message(void *socket, char *topic, json_t *message, unsigned int verbosity)
{
    char *output_buffer = json_dumps(message, JSON_COMPACT);

    if (!output_buffer) {
        printf("ERROR: Error on creating the JSON output buffer.\n");

        return EXIT_FAILURE;
    }

    if (verbosity > 0)
    {
        printf("Sending buffer: %s\n", output_buffer);
    }

    const size_t output_size = strlen(output_buffer);

    // Compute the new topic
    char new_topic[defaults_topic_buffer_size];
    // I am not sure if snprintf is standard or not, apprently it is in the C99 standard
    snprintf(new_topic, defaults_topic_buffer_size, "%s_s%zu", topic, output_size);

    if (verbosity > 0)
    {
        printf("Using topic: %s\n", new_topic);
    }

    const int result = send_byte_message(socket, new_topic, (void*)output_buffer, strlen(output_buffer), 1);
    
    free(output_buffer);

    return result;
}

inline extern int receive_json_message(void *socket, char **topic, json_t **message, bool extract_topic, unsigned int verbosity)
{
    if (!topic && extract_topic)
    {
        printf("ERROR: Topic is a null pointer\n");

        return EXIT_FAILURE;
    }
    
    if (!message)
    {
        printf("ERROR: Message is a null pointer\n");

        return EXIT_FAILURE;
    }
    
    char *input_buffer;
    size_t size;

    const int result = receive_byte_message(socket, topic, (void**)(&input_buffer), &size, extract_topic, verbosity);
    
    if (result == EXIT_FAILURE)
    {
        printf("ERROR: Some error occurred!!!\n");

        return EXIT_FAILURE;
    }
    else if (size == 0 && result == EXIT_SUCCESS)
    {
        if (verbosity > 1)
        {
            printf("No message available\n");
        }

        *message = json_object();

        return EXIT_SUCCESS;
    }
    else if (size > 0 && result == EXIT_SUCCESS)
    {
        if (verbosity > 0)
        {
            printf("Message received!!!\n");

            if (extract_topic)
            {
                printf("Topic: %s\n", *topic);
            }

            printf("Buffer: %s\n", input_buffer);
        }

        json_error_t error;
        *message = json_loadb(input_buffer, size, 0, &error);

        free(input_buffer);

        if (!(*message))
        {
            printf("ERROR: Parse error while reading config file: %s (source: %s, line: %d, column: %d, position: %d)\n", error.text, error.source, error.line, error.column, error.position);

            return EXIT_FAILURE;
        }
        else
        {
            return EXIT_SUCCESS;
        }
    }

    return EXIT_FAILURE;
}

#endif
