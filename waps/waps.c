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
#define _ISOC99_SOURCE

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
// Fot the dynamic load of libraries
#include <dlfcn.h>

#include <jansson.h>
#include <zmq.h>

#include "defaults.h"
#include "events.h"
#include "socket_functions.h"
#include "waps_data_analysis.h"
#include "common_analysis.h"
#include "CFD_calculation.h"

struct channel_parameters_t {
    // We are using an int here so we can notify that we could not find the right id
    int id;
    bool enabled;
    unsigned int charge_sensitivity;
    unsigned int pretrigger;
    unsigned int pregate;
    unsigned int gate_short;
    unsigned int gate_long;
    int gate_extra;
    pulse_polarity_t pulse_polarity;
    unsigned int pileup_threshold;
    bool CFD_enabled;
    unsigned int CFD_smooth_samples;
    double CFD_fraction;
    int CFD_delay;
    unsigned int CFD_zero_crossing_samples;
};

typedef struct channel_parameters_t channel_parameters_t;

inline extern channel_parameters_t get_parameters(channel_parameters_t *channels, size_t n, int id)
{
    for (size_t i = 0; i < n; ++i)
    {
        if (channels[i].id == id)
        {
            return channels[i];
        }
    }

    channel_parameters_t parameters;
    parameters.id = -1;

    return parameters;
}

unsigned int terminate_flag = 0;

////////////////////////////////////////////////////////////////////////////////
// Here be the dynamic loading of libraries                                   //
////////////////////////////////////////////////////////////////////////////////
typedef void* (*FSI)(void);
typedef bool (*FSE)(uint32_t samples_number,
                                   const uint16_t *samples,
                                   size_t baseline_end,
                                   uint64_t timestamp,
                                   double qshort,
                                   double qlong,
                                   double baseline,
                                   uint8_t channel,
                                   uint8_t PUR,
                                   struct event_PSD *event,
                                   void *user_data);
typedef int (*FSC)(void *user_data);

// Workaround to avoid compiler warnings
#define UNUSED(X) (void)(X)

void *dummy_init()
{
    return NULL;
}
bool dummy_select(uint32_t samples_number,
                  const uint16_t *samples,
                  size_t baseline_end,
                  uint64_t timestamp,
                  double qshort,
                  double qlong,
                  double baseline,
                  uint8_t channel,
                  uint8_t PUR,
                  struct event_PSD *event,
                  void *user_data)
{
    UNUSED(samples_number);
    UNUSED(samples);
    UNUSED(baseline_end);
    UNUSED(timestamp);
    UNUSED(qshort);
    UNUSED(qlong);
    UNUSED(baseline);
    UNUSED(channel);
    UNUSED(PUR);
    UNUSED(event);
    UNUSED(user_data);
    return true;
}
int dummy_close(void *user_data)
{
    UNUSED(user_data);
    return 0;
}

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
    printf("Datastream converter that calculates the PSD parameters from the waveforms and simulates a events stream.\n");
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
    printf("\t-b: Disable %d bit shift in the timestamp, in order to fit the fine timestamp in the int64_t value.\n", defaults_waps_fixed_point_fractional_bits);
    printf("\t-l <selection_library>: Load a user supplied library to select events.\n");
    printf("\t                        WARNING: If the library file name contains a '/' then\n");
    printf("\t                        it is loaded as a local file, otherwise it is searched\n");
    printf("\t                        in the directories defined by LD_LIBRARY_PATH\n");
    printf("\t-E <multiplication_factor>: Calculate variance of signal on the baseline window\n");

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
    char *dynamic_library_name = NULL;
    bool enable_forward = false;
    bool enable_gates = false;
    bool disable_shift = false;
    double variance_multiply = 1.0;
    bool calculate_variance = false;

    int c = 0;
    while ((c = getopt(argc, argv, "hS:P:T:wgl:vVbE:")) != -1) {
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
            case 'b':
                disable_shift = true;
                break;
            case 'l':
                dynamic_library_name = optarg;
                break;
            case 'E':
                variance_multiply = atof(optarg);
                calculate_variance = true;
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

    channel_parameters_t *const channels = (channel_parameters_t *)malloc(found_channels * sizeof(channel_parameters_t));

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
        json_t *json_charge_sensitivity = json_object_get(value, "charge_sensitivity");
        json_t *json_pretrigger = json_object_get(value, "pretrigger");
        json_t *json_pregate = json_object_get(value, "pregate");
        json_t *json_gate_short = json_object_get(value, "short_gate");
        json_t *json_gate_long = json_object_get(value, "long_gate");
        json_t *json_gate_extra = json_object_get(value, "extra_gate");
        json_t *json_pulse_polarity = json_object_get(value, "pulse_polarity");
        json_t *json_pileup_threshold = json_object_get(value, "pileup_threshold");
        json_t *json_name = json_object_get(value, "name");
        json_t *json_CFD = json_object_get(value, "CFD");

        // Let us check if all the parameters have the right type
        if (json_is_integer(json_id)
            && json_is_boolean(json_enabled)
            && json_is_integer(json_charge_sensitivity)
            && json_is_integer(json_pretrigger)
            && json_is_integer(json_pregate)
            && json_is_integer(json_gate_short)
            && json_is_integer(json_gate_long)
            && json_is_string(json_pulse_polarity)
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
                channels[channels_index].charge_sensitivity = json_integer_value(json_charge_sensitivity);
                channels[channels_index].pretrigger = json_integer_value(json_pretrigger);
                channels[channels_index].pregate = json_integer_value(json_pregate);
                channels[channels_index].gate_short = json_integer_value(json_gate_short);
                channels[channels_index].gate_long = json_integer_value(json_gate_long);
                channels[channels_index].gate_extra = json_integer_value(json_gate_extra);
                channels[channels_index].pileup_threshold = json_integer_value(json_pileup_threshold);

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

                if ((int)channels[channels_index].pretrigger - (int)channels[channels_index].pregate <= 0)
                {
                    printf("ERROR: Pregate (%ud) is bigger than pretrigger (%ud) for channel: %d\n", channels[channels_index].pregate, channels[channels_index].pretrigger, channels[channels_index].id);

                    error_flag = true;
                }

                if (json_object_size(json_CFD) > 0)
                {
                    // We have the config for a CFD
                    json_t *json_CFD_enabled = json_object_get(json_CFD, "enabled");
                    json_t *json_CFD_smooth_samples = json_object_get(json_CFD, "smooth_samples");
                    json_t *json_CFD_fraction = json_object_get(json_CFD, "fraction");
                    json_t *json_CFD_delay = json_object_get(json_CFD, "delay");
                    json_t *json_CFD_zero_crossing_samples = json_object_get(json_CFD, "zero_crossing_samples");

                    channels[channels_index].CFD_enabled = json_is_true(json_CFD_enabled);
                    channels[channels_index].CFD_smooth_samples = json_number_value(json_CFD_smooth_samples);
                    channels[channels_index].CFD_fraction = json_number_value(json_CFD_fraction);
                    channels[channels_index].CFD_delay = json_integer_value(json_CFD_delay);
                    channels[channels_index].CFD_zero_crossing_samples = json_integer_value(json_CFD_zero_crossing_samples);

                    if (channels[channels_index].CFD_smooth_samples <= 0)
                    {
                        channels[channels_index].CFD_smooth_samples = 1;
                    }
                }
                else
                {
                    channels[channels_index].CFD_enabled = false;
                    channels[channels_index].CFD_smooth_samples = 0;
                    channels[channels_index].CFD_fraction = 0;
                    channels[channels_index].CFD_delay = 0;
                    channels[channels_index].CFD_zero_crossing_samples = 0;
                    channels[channels_index].CFD_smooth_samples = 1;
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
            printf("* id: %d\n", channels[i].id);
            printf("  enabled: %s\n", channels[i].enabled ? "true" : "false");
            printf("  charge_sensitivity: %u\n", channels[i].charge_sensitivity);
            printf("  pretrigger: %u\n", channels[i].pretrigger);
            printf("  pregate: %u\n", channels[i].pregate);
            printf("  short_gate: %u\n", channels[i].gate_short);
            printf("  long_gate: %u\n", channels[i].gate_long);
            printf("  extra_gate: %u\n", channels[i].gate_extra);
            printf("  pulse_polarity: %d\n", channels[i].pulse_polarity);
            printf("  CFD enabled: %s\n", channels[i].CFD_enabled ? "true" : "false");
            printf("  CFD smooth samples: %u\n", channels[i].CFD_smooth_samples);
            printf("  CFD fraction: %g\n", channels[i].CFD_fraction);
            printf("  CFD delay: %d\n", channels[i].CFD_delay);
            printf("  CFD zero crossing samples: %u\n", channels[i].CFD_zero_crossing_samples);
        }
    }

    // We define this union in order to convert the data pointer from
    // dlsym() to a function pointer, that might not be compatible
    // see: https://en.wikipedia.org/wiki/Dynamic_loading#UNIX_(POSIX)
    union
    {
        FSI select_init;
        void *obj;
    } ASI;
    union
    {
        FSE select_event;
        void *obj;
    } ASE;
    union
    {
        FSC select_close;
        void *obj;
    } ASC;

    ASI.select_init = dummy_init;
    ASE.select_event = dummy_select;
    ASC.select_close = dummy_close;

    void *dl_handle = NULL;

    if (dynamic_library_name)
    {
        if (verbosity > 0)
        {
            printf("Loading library: %s\n", dynamic_library_name);
        }

        dl_handle = dlopen(dynamic_library_name, RTLD_LAZY);

        if (!dl_handle)
        {
            printf("ERROR: %s\n", dlerror());

            return EXIT_FAILURE;
        }
        else
        {
            // Clear any existing error
            dlerror();

            // Try to extract the symbols from the library
            void *dl_init = dlsym(dl_handle, "select_init");

            if (!dl_init)
            {
                printf("ERROR: %s\n", dlerror());

                return EXIT_FAILURE;
            }
            else
            {
                ASI.obj = dl_init;
            }

            void *dl_select = dlsym(dl_handle, "select_event");

            if (!dl_select)
            {
                printf("ERROR: %s\n", dlerror());

                return EXIT_FAILURE;
            }
            else
            {
                ASE.obj = dl_select;
            }

            void *dl_close = dlsym(dl_handle, "select_close");

            if (!dl_close)
            {
                printf("ERROR: %s\n", dlerror());

                return EXIT_FAILURE;
            }
            else
            {
                ASC.obj = dl_close;
            }

            // Clear any existing error
            dlerror();
        }
    }

    if (verbosity > 0 && dynamic_library_name)
    {
        printf("Initializing library data\n");
    }
    void *user_data = ASI.select_init();

    if (verbosity > 0 && dynamic_library_name)
    {
        printf("Library init completed\n");
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

                        uint64_t timestamp = *((uint64_t *)(input_buffer + input_offset));
                        const uint8_t this_channel = *((uint8_t *)(input_buffer + input_offset + 8));
                        const uint32_t samples_number = *((uint32_t *)(input_buffer + input_offset + 9));
                        const uint8_t gates_number = *((uint8_t *)(input_buffer + input_offset + 13));

                        if (verbosity > 1)
                        {
                            printf("[%zu] Channel: %" PRIu8 "; Samples number: %" PRIu32 "\n", events_number, this_channel, samples_number);
                        }

                        const channel_parameters_t parameters = get_parameters(channels,
                                                                               number_of_channels,
                                                                               this_channel);

                        size_t baseline_end = parameters.pretrigger - parameters.pregate;

                        // First we check all the possible errors...
                        // If the id is negative then the channel is not active.
                        if (parameters.id < 0)
                        {
                            printf("[%zu] WARNING: Channel %d is not active\n", counter, this_channel);
                        }
                        else if (baseline_end > samples_number)
                        {
                            printf("[%zu] ERROR: Baseline width is too long\n", counter);
                        }
                        else if (parameters.gate_short > samples_number)
                        {
                            printf("[%zu] ERROR: Short gate is too long\n", counter);
                        }
                        else if (parameters.gate_long > samples_number)
                        {
                            printf("[%zu] ERROR: Long gate is too long\n", counter);
                        }
                        else if (parameters.gate_extra > (int)samples_number)
                        {
                            printf("[%zu] ERROR: Extra gate is too long\n", counter);
                        }
                        // If we still have data in the buffer we can continue
                        else if ((input_offset + (samples_number * sizeof(uint16_t))) < size)
                        {
                            // Shifting the timestamp, so we can fit the fine_timestamp in the LSBs.
                            // The shift is always performed, in order not to have mixed results with both
                            // shifted and non-shifted timestamps in the same population.
                            if (!disable_shift) {
                                timestamp = (timestamp << defaults_waps_fixed_point_fractional_bits);
                            }

                            const uint16_t *samples = (uint16_t *)(input_buffer + input_offset + 14);
                            const uint16_t *gates = (uint16_t *)(input_buffer + input_offset + 14 + samples_number * gates_number * sizeof(uint8_t));

                            uint64_t *samples_integral = malloc(sizeof(uint64_t) * samples_number);
                            double *curve_integral = malloc(sizeof(double) * samples_number);
                            double *smooth_samples = malloc(sizeof(double) * samples_number);
                            double *monitor_samples = calloc(samples_number, sizeof(double));
                            double monitor_min = 0;
                            double monitor_max = 0;

                            // Flag to select event for forwwarding
                            bool is_selected = false;

                            if (!samples_integral || !curve_integral || !smooth_samples || !monitor_samples)
                            {
                                printf("[%zu] ERROR: Unable to allocate cumulative curves\n", counter);
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

                                cumulative_sum(samples, samples_number, &samples_integral);

                                uint64_t raw_baseline = samples_integral[baseline_end - 1];
                                double baseline = (double)raw_baseline / baseline_end;

                                integral_baseline_subtract(samples_integral, samples_number, baseline, &curve_integral);

                                if (parameters.CFD_enabled == true)
                                {
                                    if (verbosity > 1)
                                    {
                                        printf("[%zu] DCFD calculation start\n", events_number);
                                    }

                                    // We should be determining the fine timestamp

                                    running_mean(curve_integral, samples_number, parameters.CFD_smooth_samples, &smooth_samples);

                                    CFD_signal(smooth_samples, samples_number, parameters.CFD_delay, parameters.CFD_fraction, &monitor_samples);

                                    size_t monitor_index_min = 0;
                                    size_t monitor_index_max = 0;

                                    find_extrema(monitor_samples, 0, samples_number, &monitor_index_min, &monitor_index_max, &monitor_min, &monitor_max);

                                    if (verbosity > 1)
                                    {
                                        printf("[%zu] Monitor signal extrema, index_min: %zu, min: %f, index_max: %zu, max: %f\n", counter, monitor_index_min, monitor_min, monitor_index_max, monitor_max);
                                    }
    
                                    unsigned int zero_crossing_index = 0;

                                    size_t monitor_index_left = monitor_index_min;
                                    size_t monitor_index_right = monitor_index_max;

                                    if (monitor_index_min > monitor_index_max)
                                    {
                                        monitor_index_left = monitor_index_max;
                                        monitor_index_right = monitor_index_min;
                                    }

                                    const int z_result = find_zero_crossing(monitor_samples, monitor_index_left, monitor_index_right, &zero_crossing_index);

                                    double fine_zero_crossing = 0;

                                    find_fine_zero_crossing(monitor_samples, samples_number, zero_crossing_index, parameters.CFD_zero_crossing_samples, &fine_zero_crossing);

                                    // Converting to fixed-point number
                                    const uint64_t fine_timestamp = floor(fine_zero_crossing * (1 << defaults_waps_fixed_point_fractional_bits));

                                    timestamp = timestamp + fine_timestamp;

                                    // Fixing the start of the integration gates
                                    const size_t new_baseline_end = zero_crossing_index - parameters.pregate;

                                    if (0 < new_baseline_end && new_baseline_end < samples_number)
                                    {
                                        baseline_end = new_baseline_end;

                                        // We recalculate the baseline with the new end
                                        raw_baseline = samples_integral[baseline_end - 1];
                                        baseline = (double)raw_baseline / baseline_end;
                                    }

                                    if (verbosity > 1)
                                    {
                                        printf("[%zu] Zero crossing index: %u (success: %d), fine zero crossing: %f, fine timestamp: %" PRIu64 ", baseline end: %zu\n", events_number, zero_crossing_index, z_result == EXIT_SUCCESS, fine_zero_crossing, fine_timestamp, new_baseline_end);
                                    }
                                }

                                // N.B. That '-1' is to have the right integration window since,
                                // having a discrete domain, the integrals should be considered
                                // calculated always up to the right edge of the intervals.
                                double qshort = curve_integral[baseline_end + parameters.gate_short - 1] - curve_integral[baseline_end - 2];
                                double qlong  = curve_integral[baseline_end + parameters.gate_long - 1] - curve_integral[baseline_end - 2];
                                double qextra  = curve_integral[baseline_end + parameters.gate_extra - 1] - curve_integral[baseline_end - 2];

                                uint64_t long_qshort = 0;
                                uint64_t long_qlong = 0;
                                int64_t long_qextra = 0;

                                if (parameters.pulse_polarity == POLARITY_POSITIVE)
                                {
                                    long_qshort = (uint64_t)round(qshort);
                                    long_qlong = (uint64_t)round(qlong);
                                    long_qextra = (int64_t)round(qextra);
                                }
                                else
                                {
                                    long_qshort = (uint64_t)round(qshort * -1);
                                    long_qlong = (uint64_t)round(qlong * -1);
                                    long_qextra = (int64_t)round(qextra * -1);

				    qshort *= -1;
				    qlong *= -1;
				    qextra *= -1;
                                }

				const double scaled_qshort = qshort / pow(4, parameters.charge_sensitivity);
				const double scaled_qlong = qlong / pow(4, parameters.charge_sensitivity);
				//const double scaled_qextra = qextra / pow(4, parameters.charge_sensitivity);

                                const uint64_t temp_qshort = long_qshort >> (2 * parameters.charge_sensitivity);
                                const uint64_t temp_qlong = long_qlong >> (2 * parameters.charge_sensitivity);
                                const int64_t temp_qextra = long_qextra >> (2 * parameters.charge_sensitivity);

                                // We convert the 64 bit integers to 16 bit to simulate the digitizer data
                                uint16_t int_qshort = temp_qshort & 0xffff;
                                uint16_t int_qlong = temp_qlong & 0xffff;
                                int16_t int_qextra = temp_qextra;

                                if (temp_qshort > 0xffff)
                                {
                                    int_qshort = 0xffff;
                                }
                                if (temp_qlong > 0xffff)
                                {
                                    int_qlong = 0xffff;
                                }

                                uint16_t int_baseline = round(baseline);

                                if (calculate_variance)
                                {
                                    // We calculate the signal variance for the ENOB calculation
                                    double signal_var = 0;

                                    calculate_var(samples, baseline_end, baseline, &signal_var);

                                    const uint16_t int_var = round(signal_var * variance_multiply);

                                    int_baseline = int_var;

                                    if (verbosity > 1)
                                    {
                                        printf("[%zu] Signal variance: %f, int variance: %d\n", events_number, signal_var, int_var);
                                    }
                                }


                                const bool PUR = false;

                                struct event_PSD this_event = {timestamp,
                                                               int_qshort,
                                                               int_qlong,
                                                               (parameters.gate_extra == 0) ? int_baseline : int_qextra,
                                                               this_channel,
                                                               PUR};

                                is_selected = ASE.select_event(samples_number,
                                                               samples,
                                                               baseline_end,
                                                               timestamp,
                                                               scaled_qshort,
                                                               scaled_qlong,
                                                               baseline,
                                                               this_channel,
                                                               PUR,
                                                               &this_event,
                                                               user_data);

                                if (is_selected) {
                                    output_events[output_index] = this_event;
                                    output_index += 1;
                                }

                                if (verbosity > 1)
                                {
                                    printf("timestamp: %" PRIu64 ", channel: %" PRIu8 ", baseline: %f, int_baseline: %" PRIu16 ", qshort: %f, long_qshort: %" PRIu64", int_qshort: %" PRIu16 ", qlong: %f, long_qlong: %" PRIu64", int_qlong: %" PRIu16 ", PUR: %d\n", timestamp, this_channel, baseline, int_baseline, qshort, long_qshort, int_qshort, qlong, long_qlong, int_qlong, PUR);
                                }

                                if (samples_integral)
                                {
                                    free(samples_integral);
                                }
                                if (curve_integral)
                                {
                                    free(curve_integral);
                                }
                                if (smooth_samples)
                                {
                                    free(smooth_samples);
                                }
                            }

                            if (is_selected && enable_forward && !enable_gates)
                            {
                                const size_t this_waveform_size = 14 + samples_number * sizeof(uint16_t) + gates_number * samples_number * sizeof(uint8_t);

                                if (output_offset + this_waveform_size < waveforms_size) {
                                    memcpy(output_waveforms + output_offset,      (void*)&timestamp, sizeof(timestamp));
                                    memcpy(output_waveforms + output_offset + 8,  (void*)&this_channel, sizeof(this_channel));
                                    memcpy(output_waveforms + output_offset + 9,  (void*)&samples_number, sizeof(samples_number));
                                    memcpy(output_waveforms + output_offset + 13, (void*)&gates_number, sizeof(gates_number));
                                    memcpy(output_waveforms + output_offset + 14, (void*)samples, samples_number * sizeof(uint16_t));
                                    memcpy(output_waveforms + output_offset + 14 + samples_number * sizeof(uint16_t), (void*)gates, samples_number * gates_number * sizeof(uint8_t));

                                    output_offset += this_waveform_size;
                                }
                            }
                            else if (is_selected && enable_forward && enable_gates)
                            {
                                // Compute the gates for the waveform event
                                if (verbosity > 1)
                                {
                                    printf("[%zu] Creating gates\n", counter);
                                }
    
                                // We will be overwriting the existing gates with the ones calculated here
                                const uint8_t new_gates_number = 3;

                                const double monitor_delta = monitor_max - monitor_min;

                                uint8_t *new_gate_trace_short = malloc(samples_number * sizeof(uint8_t));
                                uint8_t *new_gate_trace_long = malloc(samples_number * sizeof(uint8_t));
                                uint8_t *new_CFD_monitor_gate = malloc(samples_number * sizeof(uint8_t));

                                const size_t this_waveform_size = 14 + samples_number * sizeof(uint16_t) + new_gates_number * samples_number * sizeof(uint8_t);

                                if (output_offset + this_waveform_size < waveforms_size && new_gate_trace_short && new_gate_trace_long) {
                                    memcpy(output_waveforms + output_offset,      (void*)&timestamp, sizeof(timestamp));
                                    memcpy(output_waveforms + output_offset + 8,  (void*)&this_channel, sizeof(this_channel));
                                    memcpy(output_waveforms + output_offset + 9,  (void*)&samples_number, sizeof(samples_number));
                                    memcpy(output_waveforms + output_offset + 13, (void*)&new_gates_number, sizeof(new_gates_number));
                                    memcpy(output_waveforms + output_offset + 14, (void*)samples, samples_number * sizeof(uint16_t));

                                    // Building the gate arrays
                                    for (uint32_t i = 0; i < samples_number; i++) {
                                        if (baseline_end <= i && i < baseline_end + parameters.gate_short)
                                        {
                                            new_gate_trace_short[i] = 255;
                                        }
                                        else
                                        {
                                            new_gate_trace_short[i] = 0;
                                        }

                                        if (baseline_end <= i && i < baseline_end + parameters.gate_long)
                                        {
                                            new_gate_trace_long[i] = 255;
                                        }
                                        else
                                        {
                                            new_gate_trace_long[i] = 0;
                                        }

                                        new_CFD_monitor_gate[i] = parameters.CFD_enabled ? (monitor_samples[i] - monitor_min) / monitor_delta * UINT8_MAX : 0;
                                    }

                                    memcpy(output_waveforms
                                           + output_offset + 14
                                           + samples_number * sizeof(uint16_t),
                                           (void*)new_gate_trace_short,
                                           samples_number * sizeof(uint8_t));
                                    memcpy(output_waveforms
                                           + output_offset + 14
                                           + samples_number * sizeof(uint16_t)
                                           + samples_number * sizeof(uint8_t),
                                           (void*)new_gate_trace_long,
                                           samples_number * sizeof(uint8_t));
                                    memcpy(output_waveforms
                                           + output_offset + 14
                                           + samples_number * sizeof(uint16_t)
                                           + 2 * samples_number * sizeof(uint8_t),
                                           (void*)new_CFD_monitor_gate,
                                           samples_number * sizeof(uint8_t));

                                    output_offset += this_waveform_size;
                                }

                                if (new_gate_trace_short) {
                                    free(new_gate_trace_short);
                                }
                                if (new_gate_trace_long) {
                                    free(new_gate_trace_long);
                                }
                                if (new_CFD_monitor_gate) {
                                    free(new_CFD_monitor_gate);
                                }
                            }

                            if (monitor_samples)
                            {
                                free(monitor_samples);
                            }
                        }

                        // Compute the waveform event size
                        const size_t this_size = 14 + samples_number * sizeof(uint16_t) + gates_number * samples_number * sizeof(uint8_t);

                        input_offset += this_size;
                        events_number += 1;
                    }

                    if (verbosity > 0)
                    {
                        printf("events_number: %zu; selected_number: %zu; ratio: %f%%\n", events_number, output_index, ((double)output_index) / events_number * 100);
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

                    if (enable_forward && output_offset == 0)
                    {
                        if (verbosity > 0)
                        {
                            printf("Empty waveforms output buffer\n");
                        }
                    }
                    else if (enable_forward && output_offset > 0)
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
    
    if (verbosity > 0 && dynamic_library_name)
    {
        printf("Cleaning library data\n");
    }
    const int scc = ASC.select_close(user_data);
    if (scc != 0)
    {
        printf("ERROR: Error on closing selection function (errno: %d)\n", scc);
        return EXIT_FAILURE;
    }
 
    if (dl_handle)
    {
        dlclose(dl_handle);
    }

    return EXIT_SUCCESS;
}
