// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#ifndef __SOCKET_FUNCTIONS_C__
#define __SOCKET_FUNCTIONS_C__ 1

#include <stdio.h>
// For malloc
#include <stdlib.h>
// For memcpy
#include <string.h>
#include <stdbool.h>

#include <zmq.h>

// Sends a message through a ZeroMQ socket, parameters:
// - socket: the pointer to the socket (INPUT);
// - topic: a pointer to a string with the topic of the message (INPUT);
// - buffer: a pointer to a buffer of data (INPUT);
// - size: the size of the buffer (INPUT);
// - verbosity: a flag to activate debug output (INPUT).
extern inline int send_byte_message(void *socket, const char* topic, void *buffer, size_t size, int verbosity)
{
    const size_t topic_size = topic ? strlen(topic) : 0;
    size_t envelope_size = 0;

    if (topic_size > 0)
    {
        envelope_size = topic_size + 1 + size;
    }
    else
    {
        envelope_size = size;
    }

    if (verbosity > 1)
    {
        printf("Topic size: %zu; Envelope size: %zu\n", topic_size, envelope_size);
    }

    // Creates an empty ØMQ message
    zmq_msg_t envelope;

    // Allocates the necessary memory for the message
    const int envelope_init_result = zmq_msg_init_size(&envelope, envelope_size);
    if (envelope_init_result != 0)
    {
        printf("ERROR: ZeroMQ Error on send, on envelope init: %s\n", zmq_strerror(errno));

        zmq_msg_close(&envelope);

        return EXIT_FAILURE;
    }
    
    // Stores the data into the message
    if (topic_size > 0)
    {
        memcpy(zmq_msg_data(&envelope), topic, topic_size);
        // Some pointer arithmetics with char pointers, as it is illegal for void pointers
        memcpy((void*)((char*)zmq_msg_data(&envelope) + topic_size), " ", 1);
        memcpy((void*)((char*)zmq_msg_data(&envelope) + 1 + topic_size), buffer, size);
    }
    else
    {
        memcpy(zmq_msg_data(&envelope), buffer, size);
    }

    // Sends the message
    const size_t envelope_send_result = zmq_msg_send(&envelope, socket, 0);
    if (envelope_send_result != envelope_size)
    {
        printf("ERROR: ZeroMQ Error on send, on envelope send: %s\n", zmq_strerror(errno));

        zmq_msg_close(&envelope);

        return EXIT_FAILURE;
    }

    // Releases message
    zmq_msg_close(&envelope);

    return EXIT_SUCCESS;
}

// Receive a message from a ZeroMQ socket, parameters:
// - socket: the pointer to the socket (INPUT);
// - topic: the address of a pointer to a string (OUTPUT),
//          the memory will be allocated in the function, it needs to be freed;
// - buffer: the address of a pointer to a buffer of data (OUTPUT);
//           the memory will be allocated in the function, it needs to be freed;
// - size: the address of the size of the buffer (OUTPUT);
// - extract_topic: if requested a topic will be extracted;
// - verbosity: a flag to activate debug output (INPUT).
extern inline int receive_byte_message(void *socket, char **topic, void **buffer, size_t *size, bool extract_topic, unsigned int verbosity)
{
    // Set size to 0 to notify that no message was received or an error occurred
    *size = 0;

    // Creates an empty ØMQ message
    zmq_msg_t envelope;

    // Initializes the memory for the message
    const int envelope_init_result = zmq_msg_init(&envelope);
    if (envelope_init_result != 0)
    {
        printf("ERROR: ZeroMQ Error on receive, on envelope init: %s\n", zmq_strerror(errno));

        return EXIT_FAILURE;
    }

    // Receives message in non-bloking mode. In case of failure it shall return -1.
    // If there are no messages available, the function shall fail with errno set to EAGAIN.
    const int envelope_receive_result = zmq_msg_recv(&envelope, socket, ZMQ_DONTWAIT);
    const int error_number = errno;

    if (envelope_receive_result < 0 && error_number != EAGAIN)
    {
        printf("ERROR: ZeroMQ Error on receive, on envelope receive: %s (errno: %d)\n", zmq_strerror(errno), error_number);

        // Release message
        zmq_msg_close (&envelope);

        return EXIT_FAILURE;
    }
    else if (envelope_receive_result < 0 && error_number == EAGAIN)
    {
        // No message available, so release message
        zmq_msg_close (&envelope);

        return EXIT_SUCCESS;
    }
    else
    {
        const size_t message_size = zmq_msg_size(&envelope);

        if (verbosity > 0)
        {
            printf("Message size: %zu\n", message_size);
        }
        
        const char *begin = (char *)zmq_msg_data(&envelope);

        if (extract_topic)
        {
            // Find the space to isolate the topic
            const char *separator = strchr(begin, ' ');

            if (separator == NULL)
            {
                printf("ERROR: Unable to find topic separator\n");

                // Release message
                zmq_msg_close (&envelope);

                return EXIT_FAILURE;
            }

            const size_t topic_size = separator - begin;

            if (verbosity > 0)
            {
                printf("Topic size: %zu; Data size: %zu\n", topic_size, message_size - topic_size - 1);
            }
        

            // Check if the topic size is not too big.
            // + 1 because whe should delete the separator
            if ((topic_size + 1) > message_size)
            {
                printf("ERROR: Topic size is too big for this message!\n");

                // Release message
                zmq_msg_close (&envelope);

                return EXIT_FAILURE;
            }

            // Allocates the topic buffer
            *topic = (char *)calloc(topic_size + 1, 1);

            if (!(*topic))
            {
                printf("ERROR: Unable to allocate the topic buffer\n");

                // Release message
                zmq_msg_close (&envelope);

                return EXIT_FAILURE;
            }

            memcpy(*topic, begin, topic_size);

            // Remeber to terminate the string!!!
            (*topic)[topic_size] = '\0';

            // Allocates the necessary buffer
            *buffer = calloc(message_size - topic_size - 1, 1);

            if (!(*buffer))
            {
                printf("ERROR: Unable to allocate the data buffer\n");

                // Release message
                zmq_msg_close (&envelope);

                return EXIT_FAILURE;
            }

            // Copies the message string
            memcpy((char *)(*buffer), begin + topic_size + 1, message_size - topic_size - 1);

            *size = message_size - topic_size - 1;
        }
        else
        {
            // Allocates the necessary buffer
            *buffer = calloc(message_size, 1);

            if (!(*buffer))
            {
                printf("ERROR: Unable to allocate the data buffer\n");

                // Release message
                zmq_msg_close (&envelope);

                return EXIT_FAILURE;
            }

            // Copies the message string
            memcpy((char *)(*buffer), begin, message_size);

            *size = message_size;
        }

        // Release message
        zmq_msg_close (&envelope);

        return EXIT_SUCCESS;
    }
}

#endif
