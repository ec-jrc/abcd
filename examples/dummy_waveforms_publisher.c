// This macro is to use nanosleep even with compilation flag: -std=c99
#define _POSIX_C_SOURCE 199309L
// This macro is to enable snprintf() in macOS
#define _C99_SOURCE
#define _ISOC99_SOURCE

// For nanosleep()
#include <time.h>
// For malloc
#include <stdlib.h>
#include <string.h>
// For ints with fixed size
#include <stdint.h>
#include <inttypes.h>
// For snprintf()
#include <stdio.h>
// For boolean datatype
#include <stdbool.h>
// For the time functions for ISO time
#include <time.h>
// For round()
#include <math.h>

#include <jansson.h>
#include <zmq.h>

#include "defaults.h"
#include "events.h"
#include "utilities_functions.h"
#include "socket_functions.h"
#include "jansson_socket_functions.h"

int main()
{
    const size_t number_of_messages = 32;
    const size_t number_of_samples = 128;
    const size_t number_of_waveforms = 64;
    const size_t number_of_channels = 2;

    const char *data_output_address = defaults_abcd_data_address;
    const char *status_output_address = defaults_abcd_status_address;

    // Creates a ØMQ context
    // A context is required by the ZeroMQ messaging library
    void *context = zmq_ctx_new();
    if (!context)
    {
        printf("ERROR: ZeroMQ Error on context creation");
        return EXIT_FAILURE;
    }

    // Creates the sockets
    // For data and status messages ABCD uses PUB sockets
    void *data_output_socket = zmq_socket(context, ZMQ_PUB);
    if (!data_output_socket)
    {
        printf("ERROR: ZeroMQ Error on data output socket creation\n");
        return EXIT_FAILURE;
    }

    void *status_output_socket = zmq_socket(context, ZMQ_PUB);
    if (!status_output_socket)
    {
        printf("ERROR: ZeroMQ Error on status output socket creation\n");
        return EXIT_FAILURE;
    }

    // Connects or binds the sockets
    // The ABCD convention is that output sockets are bound, while input sockets are connected
    const int dos = zmq_bind(data_output_socket, data_output_address);
    if (dos != 0)
    {
        printf("ERROR: ZeroMQ Error on data output socket connection: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }

    const int sos = zmq_bind(status_output_socket, status_output_address);
    if (sos != 0)
    {
        printf("ERROR: ZeroMQ Error on status output socket connection: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }

    // Wait a bit before starting to send messages to prevent the slow-joiner syndrome
    struct timespec slow_joiner_wait;
    slow_joiner_wait.tv_sec = defaults_all_slow_joiner_wait / 1000;
    slow_joiner_wait.tv_nsec = (defaults_all_slow_joiner_wait % 1000) * 1000000L;
    nanosleep(&slow_joiner_wait, NULL);

    // This is the delay between messages publications
    const unsigned int base_period = 500;
    struct timespec wait;
    wait.tv_sec = base_period / 1000;
    wait.tv_nsec = (base_period % 1000) * 1000000L;

    // Send a status message notifying that the acquisition started
    // Status messages have always a timestamp in the ISO 8601 format
    char time_buffer[1024];
    time_string(time_buffer, 1024, NULL);

    // Create an empty JSON object using the Jansson library
    json_t *json_start_message = json_object();

    // Fill the status message with entries used by ABCD to identify these messages
    json_object_set_new_nocheck(json_start_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_start_message, "event", json_string("Start acquisition"));
    json_object_set_new_nocheck(json_start_message, "module", json_string("dummy"));
    json_object_set_new_nocheck(json_start_message, "timestamp", json_string(time_buffer));
    json_object_set_new_nocheck(json_start_message, "msg_ID", json_integer(0));

    send_json_message(status_output_socket, defaults_abcd_events_topic, json_start_message, 0);

    // Destroy the JSON message
    json_decref(json_start_message);

    const time_t acquisition_start = time(NULL);

    // Allocate an output buffer for the waveforms
    uint8_t *output_buffer = malloc((waveform_header_size() + sizeof(uint16_t) * number_of_samples)
                                    * number_of_waveforms
                                    * sizeof(uint8_t));
    if (!output_buffer) {
        printf("ERROR: Unable to allocate output buffer\n");
        return EXIT_FAILURE;
    }

    for (size_t msg_ID = 0; msg_ID < number_of_messages; msg_ID++)
    {
        size_t buffer_size = 0;

        for (size_t wav_ID = 0; wav_ID < number_of_waveforms; wav_ID++)
        {
            // Creting bogus data
            const uint64_t timestamp = (msg_ID * number_of_waveforms + wav_ID) * base_period;
            const uint8_t channel = wav_ID % number_of_channels;
            const uint32_t samples_number = number_of_samples;
            const uint8_t additional_waveforms_number = 0;

            // Construct a waveform object
            struct event_waveform this_waveform = waveform_create(timestamp,
                                                                  channel,
                                                                  samples_number,
                                                                  additional_waveforms_number);

            // Construct the actual waveform samples
            uint16_t *samples = waveform_samples_get(&this_waveform);

            // Creting bogus samples that would show that something is happening
            for (size_t i = 0; i < waveform_samples_get_number(&this_waveform); i++)
            {
                if (i == msg_ID) {
                    samples[i] = i;
                } else {
                    samples[i] = 0;
                }
            }

            // Serialize the waveform and copy its data to the output buffer
            const size_t this_waveform_size = waveform_size(&this_waveform);

            memcpy(output_buffer + buffer_size,
                   (void*)waveform_serialize(&this_waveform),
                   this_waveform_size);

            buffer_size += this_waveform_size;

            // Cleaning up after the mess
            waveform_destroy_samples(&this_waveform);
        }

        // Compute the new topic
        // A topic is required by ZeroMQ PUB sockets and it is used by ABCD to identify messages
        char new_topic[defaults_all_topic_buffer_size];

        // I am not sure if snprintf is standard or not, apprently it is in the C99 standard
        snprintf(new_topic, defaults_all_topic_buffer_size, "data_abcd_waveforms_v0_s%zu", buffer_size);

        send_byte_message(data_output_socket, new_topic, (void *)output_buffer, buffer_size, 0);

        nanosleep(&wait, NULL);
    }

    free(output_buffer);

    // Send another message notifying that the acquisition ended

    const time_t acquisition_stop = time(NULL);
    const time_t acquisition_duration = (acquisition_stop - acquisition_start);

    char stop_message[1024];

    snprintf(stop_message, 1024, "Stop acquisition (duration: %ld s)", acquisition_duration);

    time_string(time_buffer, 1024, NULL);

    json_t *json_stop_message = json_object();

    json_object_set_new_nocheck(json_stop_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_stop_message, "event", json_string(stop_message));
    json_object_set_new_nocheck(json_stop_message, "module", json_string("dummy"));
    json_object_set_new_nocheck(json_stop_message, "timestamp", json_string(time_buffer));
    json_object_set_new_nocheck(json_stop_message, "msg_ID", json_integer(1));

    send_json_message(status_output_socket, defaults_abcd_events_topic, json_stop_message, 0);

    json_decref(json_stop_message);

    // Wait a bit to allow the sockets to deliver
    nanosleep(&slow_joiner_wait, NULL);

    // Cleaning up the ZeroMQ sockets
    const int doc = zmq_close(data_output_socket);
    if (doc != 0)
    {
        printf("ERROR: ZeroMQ Error on data output socket close: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }
    
    const int soc = zmq_close(status_output_socket);
    if (soc != 0)
    {
        printf("ERROR: ZeroMQ Error on status output socket close: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }
    
    const int cc = zmq_ctx_destroy(context);
    if (cc != 0)
    {
        printf("ERROR: ZeroMQ Error on context destroy: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
