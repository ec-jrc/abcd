/*
 * (C) Copyright 2016 Cristiano Lino Fontana
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

// This macro is to use nanosleep even with compilation flag: -std=c99
#define _POSIX_C_SOURCE 199309L
// This macro is to enable snprintf() in macOS
#define _C99_SOURCE

// For nanosleep()
#include <time.h>
// Fot getopt
#include <getopt.h>
// For malloc
#include <stdlib.h>
#include <string.h>
// For ints with fixed size
#include <stdint.h>
#include <inttypes.h>
// For kernel signals management
#include <signal.h>
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
#include "socket_functions.h"
#include "common_analysis.h"
#include "waph_data_analysis.h"

struct waph_channel_parameters_t {
    // We are using an int here so we can notify that we could not find the right id
    int id;
    bool enabled;
    unsigned int trapezoid_risetime;
    unsigned int trapezoid_flattop;
    unsigned int trapezoid_rescaling;
    unsigned int peaking_time;
    unsigned int baseline_window;
    unsigned int decay_time;
    unsigned int pulse_polarity;
};

typedef struct waph_channel_parameters_t waph_channel_parameters_t;

inline waph_channel_parameters_t get_parameters(waph_channel_parameters_t *channels, size_t n, int id)
{
    for (size_t i = 0; i < n; ++i)
    {
        if (channels[i].id == id)
        {
            return channels[i];
        }
    }

    waph_channel_parameters_t parameters;
    parameters.id = -1;

    return parameters;
}

unsigned int terminate_flag = 0;

// Handle standard signals
// SIGTERM (from kill): terminates kindly forcing the status to the closing branch of the state machine.
// SIGINT (from ctrl-c): same behaviour as SIGTERM
// SIGHUP (from shell processes): same behaviour as SIGTERM

void signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM || signum == SIGHUP)
    {
        terminate_flag = 1;
    }
}

void print_usage(const char *name) {
    printf("Usage: %s [options] <config_file>\n", name);
    printf("\n");
    printf("Datastream converter that calculates the pulse height with a trapezoidal filter from the waveforms and simulates a events stream.\n");
    printf("A config file is required.\n");
    printf("\n");
    printf("Optional arguments:\n");
    printf("\t-h: Display this message\n");
    printf("\t-v: Set verbose execution\n");
    printf("\t-V: Set verbose execution with more output\n");
    printf("\t-S <address>: SUB socket address, default: %s\n", defaults_abcd_data_address_sub);
    printf("\t-P <address>: PUB socket address, default: %s\n", defaults_waps_data_address);
    printf("\t-T <period>: Set base period in milliseconds, default: %d\n", defaults_waps_base_period);
    printf("\t-w: Enable waveforms forwarding.\n");
    printf("\t-g: Enable gates in the forwarded waveforms.\n");

    return;
}

int main(int argc, char *argv[])
{
    // Register the handler for SIGTERM (from kill), SIGINT (from ctrl-c)
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);

    unsigned int verbosity = 0;
    unsigned int base_period = defaults_waps_base_period;
    char *input_address = defaults_abcd_data_address_sub;
    char *output_address = defaults_waps_data_address;
    bool enable_forward = false;
    bool enable_gates = false;

    int c = 0;
    while ((c = getopt(argc, argv, "hS:P:T:wgvV")) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'S':
                input_address = optarg;
                break;
            case 'P':
                output_address = optarg;
                break;
            case 'T':
                base_period = atoi(optarg);
                break;
            case 'v':
                verbosity = 1;
                break;
            case 'V':
                verbosity = 2;
                break;
            case 'w':
                enable_forward = true;
                break;
            case 'g':
                enable_gates = true;
                break;
            default:
                printf("Unknown command: %c", c);
                break;
        }
    }

    const int number_of_arguments = argc - optind;

    if (number_of_arguments < 1)
    {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    const char *config_file_name = argv[optind];

    json_error_t error;
    json_t *json_config = json_load_file(config_file_name, 0, &error);

    if (!json_config)
    {
        printf("ERROR: Parse error while reading config file: %s (source: %s, line: %d, column: %d, position: %d)\n", error.text, error.source, error.line, error.column, error.position);

        return EXIT_FAILURE;
    }

    
    // Reading the JSON array with all the channels configs
    json_t *json_channels = json_object_get(json_config, "channels");

    // It should be safe to check it directly like this,
    // because json_array_size() should just return 0 if it is not a valid array.
    const size_t found_channels = json_array_size(json_channels);

    if (found_channels == 0)
    {
        printf("ERROR: Unable to find a filled 'channels' array\n");

        json_decref(json_config);

        return EXIT_FAILURE;
    }

    waph_channel_parameters_t *const channels = (waph_channel_parameters_t *)malloc(found_channels * sizeof(waph_channel_parameters_t));

    if (!channels)
    {
        printf("ERROR: Unable to allocate memory for the channels parameters\n");

        json_decref(json_config);

        return EXIT_FAILURE;
    }

    bool error_flag = false;

    size_t channels_index = 0;
    size_t index = 0;
    json_t *value = NULL;

    json_array_foreach(json_channels, index, value) {
        // This block of code uses the index and value variables
        // that are passed to the json_array_foreach() macro.
        //
        json_t *json_id = json_object_get(value, "id");
        json_t *json_enabled = json_object_get(value, "enabled");
        json_t *json_trapezoid_risetime = json_object_get(value, "trapezoid_risetime");
        json_t *json_trapezoid_flattop = json_object_get(value, "trapezoid_flattop");
        json_t *json_trapezoid_rescaling = json_object_get(value, "trapezoid_rescaling");
        json_t *json_decay_time = json_object_get(value, "decay_time");
        json_t *json_peaking_time = json_object_get(value, "peaking_time");
        json_t *json_baseline_window = json_object_get(value, "baseline_window");
        json_t *json_pulse_polarity = json_object_get(value, "pulse_polarity");
        json_t *json_name = json_object_get(value, "name");

        // Let us check if all the parameters have the right type
        if (json_is_integer(json_id)
            && json_is_boolean(json_enabled)
            && json_is_integer(json_trapezoid_risetime)
            && json_is_integer(json_trapezoid_flattop)
            && json_is_integer(json_trapezoid_rescaling)
            && json_is_integer(json_decay_time)
            && json_is_integer(json_peaking_time)
            // The baseline may also be ignored and left to zero
            //&& json_is_integer(json_baseline_window)
            //&& json_is_integer(json_pileup_threshold)
            )
        {
            if (verbosity > 0)
            {
                printf("Found channel id: %lld, name: %s\n", json_integer_value(json_id), json_string_value(json_name));
            }

            // If the channel is enabled we can save its parameters
            if (json_is_true(json_enabled))
            {
                // If the channel was already found we exit in panic
                if (get_parameters(channels, channels_index, json_integer_value(json_id)).id >= 0)
                {
                    printf("ERROR: Duplicated config for channel: %lld\n", json_integer_value(json_id));

                    error_flag = true;
                }

                channels[channels_index].id = json_integer_value(json_id);
                channels[channels_index].trapezoid_risetime = json_integer_value(json_trapezoid_risetime);
                channels[channels_index].trapezoid_flattop = json_integer_value(json_trapezoid_flattop);
                channels[channels_index].trapezoid_rescaling = json_integer_value(json_trapezoid_rescaling);
                channels[channels_index].decay_time = json_integer_value(json_decay_time);
                channels[channels_index].peaking_time = json_integer_value(json_peaking_time);
                channels[channels_index].baseline_window = json_integer_value(json_baseline_window);

                // If the polarity description is not right we exit in panic
                if (strstr(json_string_value(json_pulse_polarity), "Negative") ||
                    strstr(json_string_value(json_pulse_polarity), "negative"))
                {
                    channels[channels_index].pulse_polarity = POLARITY_NEGATIVE;
                }
                else if (strstr(json_string_value(json_pulse_polarity), "Positive") ||
                         strstr(json_string_value(json_pulse_polarity), "positive"))
                {
                    channels[channels_index].pulse_polarity = POLARITY_POSITIVE;
                }
                else
                {
                    printf("ERROR: Invalid pulse polarity for channel: %lld (should be 'negative' or 'positive', got: %s)\n", json_integer_value(json_id), json_string_value(json_pulse_polarity));
                }

                channels_index += 1;
            }
        }
    }

    const size_t number_of_channels = channels_index;

    json_decref(json_config);

    if (error_flag && channels)
    {
        free(channels);

        return EXIT_FAILURE;
    }

    if (verbosity > 0) {
        printf("Input socket address: %s\n", input_address);
        printf("Output socket address: %s\n", output_address);
        printf("Verbosity: %u\n", verbosity);
        printf("Base period: %u\n", base_period);
        printf("Valid channel parameters: %zu\n", number_of_channels);
        printf("Channels:\n");
        for (size_t i = 0; i < number_of_channels; ++i)
        {
            printf("\tid: %d\n", channels[i].id);
            printf("\ttrapezoid_risetime: %u\n", channels[i].trapezoid_risetime);
            printf("\ttrapezoid_flattop: %u\n", channels[i].trapezoid_flattop);
            printf("\ttrapezoid_rescaling: %u\n", channels[i].trapezoid_rescaling);
            printf("\tdecay_time: %u\n", channels[i].decay_time);
            printf("\tpeaking_time: %u\n", channels[i].peaking_time);
            printf("\tbaseline_window: %u\n", channels[i].baseline_window);

            if (channels[i].pulse_polarity == POLARITY_POSITIVE)
            {
                printf("\tpulse_polarity: positive\n");
            }
            else
            {
                printf("\tpulse_polarity: negative\n");
            }
        }
    }

    // Creates a ØMQ context
    void *context = zmq_ctx_new();
    if (!context)
    {
        printf("ERROR: ZeroMQ Error on context creation");
        return EXIT_FAILURE;
    }

    void *input_socket = zmq_socket(context, ZMQ_SUB);
    if (!input_socket)
    {
        printf("ERROR: ZeroMQ Error on input socket creation\n");
        return EXIT_FAILURE;
    }

    void *output_socket = zmq_socket(context, ZMQ_PUB);
    if (!output_socket)
    {
        printf("ERROR: ZeroMQ Error on output socket creation\n");
        return EXIT_FAILURE;
    }

    const int is = zmq_connect(input_socket, input_address);
    if (is != 0)
    {
        printf("ERROR: ZeroMQ Error on input socket binding: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }

    const int os = zmq_bind(output_socket, output_address);
    if (os != 0)
    {
        printf("ERROR: ZeroMQ Error on output socket connection: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }

    // Subscribe to data topic
    zmq_setsockopt(input_socket, ZMQ_SUBSCRIBE, defaults_abcd_data_waveforms_topic, strlen(defaults_abcd_data_waveforms_topic));

    // Wait a bit to prevent the slow-joiner syndrome
    struct timespec slow_joiner_wait;
    slow_joiner_wait.tv_sec = defaults_all_slow_joiner_wait / 1000;
    slow_joiner_wait.tv_nsec = (defaults_all_slow_joiner_wait % 1000) * 1000000L;
    nanosleep(&slow_joiner_wait, NULL);
    //usleep(defaults_all_slow_joiner_wait * 1000);

    // Setting up the wait for the main loop
    // If you're asking why, on earth, are we using something so complicated,
    // on the man page of usleep it is said:
    // 4.3BSD, POSIX.1-2001.  POSIX.1-2001  declares  this  function  obsolete;  use  nanosleep(2)
    struct timespec wait;
    wait.tv_sec = base_period / 1000;
    wait.tv_nsec = (base_period % 1000) * 1000000L;

    size_t counter = 0;
    size_t msg_counter = 0;
    size_t msg_ID = 0;

    while (terminate_flag == 0)
    {
        char *topic;
        uint8_t *input_buffer;
        size_t size;

        const int result = receive_byte_message(input_socket, &topic, (void **)(&input_buffer), &size, true, verbosity);

        if (result == EXIT_FAILURE)
        {
            printf("[%zu] ERROR: Some error occurred!!!\n", counter);
        }
        else if (size == 0 && result == EXIT_SUCCESS)
        {
            if (verbosity > 2)
            {
                printf("[%zu] No message available\n", counter);
            }
        }
        else if (size > 0 && result == EXIT_SUCCESS)
        {
            if (verbosity > 0)
            {
                printf("[%zu] Message received!!! (topic: %s)\n", counter, topic);
            }

            // Check if the topic is the right one
            if (strstr(topic, "data_abcd_waveforms_v0") == topic)
            {
                const clock_t event_start = clock();

                if (enable_forward && !enable_gates)
                {
                    if (verbosity > 0)
                    {
                        printf("[%zu] Forwarding message\n", counter);
                    }

                    send_byte_message(output_socket, topic, (void *)input_buffer, size, verbosity);
                }

                size_t events_number = 0;
                const size_t waveforms_size = size * defaults_waps_waveforms_buffer_multiplier;

                // We create an output buffer as big as the input buffer
                // that is most likely to be big enough for the output data
                // as it is reduced in size with the integrals.
                // The cases in which this approach fails is when the majority
                // of the input waveforms have less than two samples.
                struct event_PSD *const output_events = malloc(size);
                // We create an output buffer twice as big as the input buffer
                // that is most likely to be big enough for the output waveforms
                // data as the gates are half as big as the samples.
                uint8_t *const output_waveforms = malloc(waveforms_size);

                if (!output_events || !output_waveforms)
                {
                    printf("[%zu] ERROR: Unable to allocate output buffer\n", counter);
                }
                else
                {
                    size_t input_offset = 0;
                    size_t output_offset = 0;
                    size_t output_index = 0;
                
                    // We add 14 bytes to the input_offset because we want to be sure to read at least
                    // the first header of the waveform
                    while ((input_offset + 14) < size && (output_index * sizeof(struct event_PSD)) < size)
                    {
                        if (verbosity > 2)
                        {
                            printf("size: %zu, input_offset: %zu\n", size, input_offset);
                        }

                        const uint64_t timestamp = *((uint64_t *)(input_buffer + input_offset));
                        const uint8_t this_channel = *((uint8_t *)(input_buffer + input_offset + 8));
                        const uint32_t samples_number = *((uint32_t *)(input_buffer + input_offset + 9));
                        const uint8_t gates_number = *((uint8_t *)(input_buffer + input_offset + 13));

                        if (verbosity > 1)
                        {
                            printf("[%zu] Channel: %" PRIu8 "; Samples number: %" PRIu32 "\n", events_number, this_channel, samples_number);
                        }

                        const waph_channel_parameters_t parameters = get_parameters(channels,
                                                                                    number_of_channels,
                                                                                    this_channel);

                        // First we check all the possible errors...
                        // If the id is negative then the channel is not active.
                        if (parameters.id < 0)
                        {
                            printf("[%zu] WARNING: Channel %d is not active\n", counter, this_channel);
                        }
                        else if (parameters.peaking_time >= samples_number)
                        {
                            printf("[%zu] WARNING: Peaking time is bigger than samples number for channel %d.\n", counter, this_channel);
                        }
                        else if (parameters.baseline_window >= samples_number)
                        {
                            printf("[%zu] WARNING: Baseline window is bigger than samples number for channel %d.\n", counter, this_channel);
                        }
                        // If we still have data in the buffer we can continue
                        else if ((input_offset + (samples_number * sizeof(uint16_t))) < size)
                        {
                            const uint16_t *samples = (uint16_t *)(input_buffer + input_offset + 14);

                            double * compensated_samples = malloc(sizeof(double) * samples_number);
                            double * filtered_samples = malloc(sizeof(double) * samples_number);

                            double trapezoid_height = 0;
                            double trapezoid_base = 0;

                            if (!filtered_samples || !compensated_samples)
                            {
                                printf("[%zu] ERROR: Unable to allocate output curves\n", counter);
                            }
                            else
                            {
                                ////////////////////////////////////////////////////
                                // Here be all the analysis!!!                    //
                                ////////////////////////////////////////////////////
                                //const bool PUR = is_pileup(samples,
                                //                           samples_number,
                                //                           parameters.pretrigger,
                                //                           parameters.pileup_threshold);
                                pole_zero_correction(samples, samples_number, parameters.decay_time, parameters.pulse_polarity, &compensated_samples);

                                trapezoidal_filter(compensated_samples, samples_number, parameters.trapezoid_risetime, parameters.trapezoid_flattop, parameters.pulse_polarity, &filtered_samples);

                                size_t trapezoid_index_height = 0;
                                size_t trapezoid_index_base = 0;

                                find_extrema(filtered_samples, 0, samples_number, &trapezoid_index_base, &trapezoid_index_height, &trapezoid_base, &trapezoid_height);

                                double peak_height = filtered_samples[parameters.peaking_time];

                                double baseline_value = 0;

                                sum(filtered_samples, parameters.baseline_window, &baseline_value);

                                // Correcting by the baseline
                                peak_height -= baseline_value;
                                trapezoid_height -= baseline_value;

                                double rescaled_trapezoid_height = (double)trapezoid_height / (1 << parameters.trapezoid_rescaling);
                                double rescaled_peak_height = (double)peak_height / (1 << parameters.trapezoid_rescaling);

                                // We convert the 64 bit integers to 16 bit to simulate the digitizer data
                                uint16_t int_trapezoid_height = 0;

                                if (rescaled_trapezoid_height > 0xffff)
                                {
                                    int_trapezoid_height = 0xffff;
                                }
                                else if (rescaled_trapezoid_height < 0)
                                {
                                    int_trapezoid_height = 0;
                                }
                                else
                                {
                                    int_trapezoid_height = (uint16_t)rescaled_trapezoid_height;
                                }

                                uint16_t int_peak_height = 0;

                                if (rescaled_peak_height > 0xffff)
                                {
                                    int_peak_height = 0xffff;
                                }
                                else if (rescaled_peak_height < 0)
                                {
                                    int_peak_height = 0;
                                }
                                else
                                {
                                    int_peak_height = (uint16_t)rescaled_peak_height;
                                }

                                uint16_t int_baseline = 0;
                                if (parameters.baseline_window == 0)
                                {
                                    int_baseline = (int16_t)round(filtered_samples[0]);
                                }
                                else
                                {
                                    int_baseline = (int16_t)round(baseline_value);
                                }

                                const struct event_PSD this_event = {timestamp, int_trapezoid_height, int_peak_height, int_baseline, this_channel, 0};

                                output_events[output_index] = this_event;
                                output_index += 1;

                                if (verbosity > 1)
                                {
                                    printf("timestamp: %" PRIu64 ", channel: %" PRIu8 ", baseline: %g, peak_height: %g, int_peak_height: %" PRIu16 ", trapezoid_height: %g, int_trapezoid_height: %" PRIu16 ", trapezoid_base: %g\n", timestamp, this_channel, baseline_value, peak_height, int_peak_height, trapezoid_height, int_trapezoid_height, trapezoid_base);
                                }
                            }

                            // Compute the gates for the waveform event
                            if (enable_forward && enable_gates)
                            {
                                if (verbosity > 1)
                                {
                                    printf("[%zu] Creating gates\n", counter);
                                }
    
                                // We will be overwriting the existing gates with the ones calculated here
                                const uint8_t new_gates_number = 2;

                                uint8_t *const compensated_gate = malloc(samples_number * sizeof(uint8_t));
                                uint8_t *const filtered_gate = malloc(samples_number * sizeof(uint8_t));

                                const size_t this_waveform_size = 14 + samples_number * sizeof(uint16_t) + new_gates_number * samples_number * sizeof(uint8_t);

                                if (output_offset + this_waveform_size < waveforms_size && compensated_gate && filtered_gate) {
                                    memcpy(output_waveforms + output_offset,      (void*)&timestamp, sizeof(timestamp));
                                    memcpy(output_waveforms + output_offset + 8,  (void*)&this_channel, sizeof(this_channel));
                                    memcpy(output_waveforms + output_offset + 9,  (void*)&samples_number, sizeof(samples_number));
                                    memcpy(output_waveforms + output_offset + 13, (void*)&new_gates_number, sizeof(new_gates_number));
                                    memcpy(output_waveforms + output_offset + 14, (void*)samples, samples_number * sizeof(uint16_t));

                                    double pulse_height = 0;
                                    double pulse_base = 0;
                                    size_t pulse_index_height = 0;
                                    size_t pulse_index_base = 0;

                                    find_extrema(compensated_samples, 0, samples_number, &pulse_index_base, &pulse_index_height, &pulse_base, &pulse_height);

                                    // Building the gate arrays
                                    for (uint32_t i = 0; i < samples_number; i++) {
                                        compensated_gate[i] = (compensated_samples[i] - pulse_base) / pulse_height * INT8_MAX;
                                        filtered_gate[i] = (filtered_samples[i] - trapezoid_base) / trapezoid_height * INT8_MAX;
                                    }

                                    memcpy(output_waveforms + output_offset + 14 + samples_number * sizeof(uint16_t), (void*)compensated_gate, samples_number * sizeof(uint8_t));
                                    memcpy(output_waveforms + output_offset + 14 + samples_number * sizeof(uint16_t) + samples_number * sizeof(uint8_t), (void*)filtered_gate, samples_number * sizeof(uint8_t));

                                    output_offset += this_waveform_size;
                                }

                                if (compensated_gate) {
                                    free(compensated_gate);
                                }
                                if (filtered_gate) {
                                    free(filtered_gate);
                                }
                            }

                            if (compensated_samples)
                            {
                                free(compensated_samples);
                            }
                            if (filtered_samples)
                            {
                                free(filtered_samples);
                            }
                        }

                        // Compute the waveform event size
                        const size_t this_size = 14 + samples_number * sizeof(uint16_t) + gates_number * samples_number * sizeof(uint8_t);

                        input_offset += this_size;
                        events_number += 1;
                    }

                    if (output_index == 0)
                    {
                        if (verbosity > 0)
                        {
                            printf("Empty output buffer\n");
                        }
                    }
                    else
                    {
                        const size_t output_size = output_index * sizeof(struct event_PSD);

                        // Compute the new topic
                        char new_topic[defaults_all_topic_buffer_size];
                        // I am not sure if snprintf is standard or not, apprently it is in the C99 standard
                        snprintf(new_topic, defaults_all_topic_buffer_size, "data_abcd_events_v0_s%zu", output_size);

                        send_byte_message(output_socket, new_topic, (void *)output_events, output_size, verbosity);
                        msg_ID += 1;

                        if (verbosity > 0)
                        {
                            printf("Sending message with topic: %s\n", new_topic);
                        }
                    }

                    if (enable_forward && enable_gates && output_offset == 0)
                    {
                        if (verbosity > 0)
                        {
                            printf("Empty waveforms output buffer\n");
                        }
                    }
                    else if (enable_forward && enable_gates && output_offset > 0)
                    {
                        // Compute the new topic
                        char new_topic[defaults_all_topic_buffer_size];
                        // I am not sure if snprintf is standard or not, apprently it is in the C99 standard
                        snprintf(new_topic, defaults_all_topic_buffer_size, "data_abcd_waveforms_v0_s%zu", output_offset);

                        send_byte_message(output_socket, new_topic, (void *)output_waveforms, output_offset, verbosity);
                        msg_ID += 1;

                        if (verbosity > 0)
                        {
                            printf("Sending waveforms message with topic: %s\n", new_topic);
                        }
                    }
                }

                if (output_events) {
                    free(output_events);
                }
                if (output_waveforms) {
                    free(output_waveforms);
                }

                const clock_t event_stop = clock();

                if (verbosity > 0)
                {
                    const float elaboration_time = (float)(event_stop - event_start) / CLOCKS_PER_SEC * 1000;
                    const float elaboration_speed = size / elaboration_time * 1000.0 / 1024.0 / 1024.0;
                    const float elaboration_rate = events_number / elaboration_time * 1000.0;

                    printf("size: %zu; events_number: %zu; elaboration_time: %f ms; elaboration_speed: %f MBi/s, %f evts/s\n", size, events_number, elaboration_time, elaboration_speed, elaboration_rate);
                }
            }

            msg_counter += 1;

            // Remember to free buffers
            free(topic);
            free(input_buffer);
        }
        else
        {
            printf("[%zu] ERROR: What?!?!?!\n", counter);
        }

        counter += 1;

        // Putting a delay in order not to fill-up the queues
        nanosleep(&wait, NULL);
        //usleep(base_period * 1000);

        if (verbosity > 2)
        {
            printf("counter: %zu; msg_counter: %zu, msg_ID: %zu\n", counter, msg_counter, msg_ID);
        }
    }

    // Wait a bit to allow the sockets to deliver
    nanosleep(&slow_joiner_wait, NULL);
    //usleep(defaults_all_slow_joiner_wait * 1000);

    if (channels)
    {
        free(channels);
    }

    const int ic = zmq_close(input_socket);
    if (ic != 0)
    {
        printf("ERROR: ZeroMQ Error on input socket close: %s\n", zmq_strerror(errno));
        return EXIT_FAILURE;
    }
    
    const int oc = zmq_close(output_socket);
    if (oc != 0)
    {
        printf("ERROR: ZeroMQ Error on output socket close: %s\n", zmq_strerror(errno));
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
