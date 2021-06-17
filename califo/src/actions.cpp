// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <cstring>
#include <chrono>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <ctime>

extern "C" {
#include <zmq.h>
#include <gsl/gsl_rng.h>
#include <jansson.h>
}

#include "typedefs.hpp"
#include "states.hpp"
#include "events.hpp"
#include "actions.hpp"

#define counter_type double

extern "C" {
#include "utilities_functions.h"
#include "socket_functions.h"
#include "jansson_socket_functions.h"
#include "histogram.h"
#include "fit_functions.h"
}

#include "background.hpp"

#define BUFFER_SIZE 32

/******************************************************************************/
/* Generic actions                                                            */
/******************************************************************************/

void actions::generic::clear_memory(status &global_status)
{
    if (global_status.rng)
    {
        gsl_rng_free(global_status.rng);
    }

    global_status.enabled_channels.clear();
    global_status.peaks.clear();
    global_status.peak_tolerances.clear();
    global_status.last_fitted_peaks.clear();
    global_status.last_scale_factors.clear();

    for (auto &pair: global_status.histos_E)
    {
        histogram_t *const histo_E = pair.second;

        if (histo_E != NULL)
        {
            histogram_destroy(histo_E);
        }
    }

    global_status.histos_E.clear();
    global_status.fifoes.clear();
}

void actions::generic::publish_message(status &global_status,
                                      std::string topic,
                                      json_t *status_message)
{
    void *status_socket = global_status.status_socket;
    const unsigned long int status_msg_ID = global_status.status_msg_ID;

    char time_buffer[BUFFER_SIZE];
    time_string(time_buffer, BUFFER_SIZE, NULL);

    json_object_set_new(status_message, "module", json_string("califo"));
    json_object_set_new(status_message, "timestamp", json_string(time_buffer));
    json_object_set_new(status_message, "msg_ID", json_integer(status_msg_ID));

    send_json_message(status_socket, const_cast<char*>(topic.c_str()), status_message, 1);

    global_status.status_msg_ID += 1;
}

bool actions::generic::publish_status(status &global_status)
{
    json_t *status_message = json_object();
    if (status_message == NULL)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to create status_message json; ";
        std::cout << std::endl;

        // I am not sure what to do here...
        return false;
    }

    json_t *enabled_channels = json_array();
    if (enabled_channels == NULL)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to create enabled_channels json; ";
        std::cout << std::endl;

        json_decref(status_message);

        // I am not sure what to do here...
        return false;
    }

    json_t *active_channels = json_array();
    if (active_channels == NULL)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to create active_channels json; ";
        std::cout << std::endl;

        json_decref(status_message);
        json_decref(enabled_channels);

        // I am not sure what to do here...
        return false;
    }

    json_t *channels = json_array();
    if (channels == NULL)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to create channels json; ";
        std::cout << std::endl;

        json_decref(status_message);
        json_decref(enabled_channels);
        json_decref(active_channels);

        // I am not sure what to do here...
        return false;
    }

    for (auto &pair: global_status.histos_E)
    {
        const unsigned int channel = pair.first;
        histogram_t *const histo_E = pair.second;

        if (histo_E != NULL) {
            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Publishing status for active channel: " << channel << "; ";
                std::cout << std::endl;
            }

            json_t *histo_config = histogram_config_to_json(histo_E);

            json_object_set_new_nocheck(histo_config, "id", json_integer(channel));
            json_object_set_new_nocheck(histo_config, "enabled", json_false());

            const double scale_factor = global_status.last_scale_factors[channel];
            json_object_set_new_nocheck(histo_config, "scale_factor", json_real(scale_factor));

            const double peak_position = global_status.peaks[channel].mu;
            const double fitted_peak_position = global_status.last_fitted_peaks[channel].mu;
            const double peak_width = global_status.peaks[channel].sigma;
            const double fitted_peak_width = global_status.last_fitted_peaks[channel].sigma;
            const double peak_tolerance = global_status.peak_tolerances[channel];

            json_t *json_peak = json_object();

            json_object_set_new_nocheck(json_peak, "position", json_real(peak_position));
            json_object_set_new_nocheck(json_peak, "fitted_position", json_real(fitted_peak_position));
            json_object_set_new_nocheck(json_peak, "width", json_real(peak_width));
            json_object_set_new_nocheck(json_peak, "fitted_width", json_real(fitted_peak_width));
            json_object_set_new_nocheck(json_peak, "tolerance", json_real(peak_tolerance));

            json_object_set_new_nocheck(histo_config, "peak", json_peak);

            json_array_append_new(active_channels, json_integer(channel));
            json_array_append_new(channels, histo_config);
        }
    }

    json_object_set_new_nocheck(status_message, "channels", channels);
    json_object_set_new_nocheck(status_message, "enabled_channels", enabled_channels);
    json_object_set_new_nocheck(status_message, "active_channels", active_channels);

    actions::generic::publish_message(global_status, defaults_califo_status_topic, status_message);

    json_decref(status_message);

    return true;
}

bool actions::generic::read_socket(status &global_status)
{
    void *abcd_data_socket = global_status.abcd_data_socket;

    char *topic;
    char *input_buffer;
    size_t size;

    int result = receive_byte_message(abcd_data_socket, &topic, (void **)(&input_buffer), &size, true, global_status.verbosity);

    while (size > 0 && result == EXIT_SUCCESS)
    {
        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Message size: " << size << "; ";
            std::cout << std::endl;
        }

        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Topic: " << topic << "; ";
            std::cout << std::endl;
        }

        const std::string topic_string(topic);

        if (topic_string.find(defaults_abcd_data_events_topic) == 0)
        {
            const clock_t event_start = clock();

            const size_t data_size = size;

            // Clearing histograms
            for (auto &pair: global_status.histos_E)
            {
                histogram_t *const histo_E = pair.second;
        
                if (histo_E != NULL)
                {
                    histogram_reset(histo_E);
                }
            }

            const size_t events_number = data_size / sizeof(event_PSD);

            std::vector<event_PSD> events_buffer;
            events_buffer.reserve(events_number);

            size_t found_events = 0;

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Data size: " << data_size << "; ";
                std::cout << "Events number: " << events_number << "; ";
                std::cout << "mod: " << data_size % sizeof(event_PSD) << "; ";
                std::cout << std::endl;
            }

            event_PSD *events = reinterpret_cast<event_PSD*>(input_buffer);

            for (size_t i = 0; i < events_number; i++)
            {
                //std::cout << "i: " << i << "; Events number: " << events_number << "; " << std::endl;
                const event_PSD this_event = events[i];

                // If it is a enabled channel we can look for the coincidences
                if (std::find(global_status.enabled_channels.begin(),
                              global_status.enabled_channels.end(),
                              this_event.channel) != global_status.enabled_channels.end())
                {
                    histogram_fill(global_status.histos_E[this_event.channel], this_event.qlong);
                    
                    const double scale_factor = global_status.last_scale_factors[this_event.channel];

                    const double smear = gsl_rng_uniform(global_status.rng);
                    const uint16_t new_qshort = (this_event.qshort + smear) * scale_factor;
                    const uint16_t new_qlong  = (this_event.qlong  + smear) * scale_factor;

                    //std::cout << "i: " << i << "; qlong: " << this_event.qlong << "; new_qlong: " << new_qlong << std::endl;

                    const event_PSD new_event = {   this_event.timestamp,
                                                    new_qshort,
                                                    new_qlong,
                                                    this_event.baseline,
                                                    this_event.channel,
                                                    this_event.pur
                                                };

                    events_buffer.push_back(new_event);
    
                    found_events += 1;
                }
            }

            // Storing histograms to the FIFOes
            for (auto &pair: global_status.histos_E)
            {
                const unsigned int channel = pair.first;
                histogram_t *const histo_E = pair.second;
        
                if (histo_E != NULL)
                {
                    const size_t histo_size = histo_E->bins * sizeof(counter_type) / sizeof(uint8_t);

                    binary_fifo::binary_data new_data;
                    new_data.resize(histo_size * sizeof(uint8_t));
        
                    memcpy(new_data.data(), histo_E->histo, histo_size * sizeof(uint8_t));
        
                    global_status.fifoes[channel].push(new_data);
                }
            }

            const size_t buffer_size = events_buffer.size();

            if (buffer_size > 0)
            {
                const size_t data_size = buffer_size * sizeof(event_PSD);
                
                std::string topic = defaults_abcd_data_events_topic;
                topic += "_v0_s";
                topic += std::to_string(data_size);

                if (global_status.verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Sending binary buffer; ";
                    std::cout << "Topic: " << topic << "; ";
                    std::cout << "events: " << buffer_size << "; ";
                    std::cout << "buffer size: " << data_size << "; ";
                    std::cout << std::endl;
                }

                const int result = send_byte_message(global_status.data_socket, \
                                                     topic.c_str(), \
                                                     events_buffer.data(), \
                                                     data_size, 0);

                if (result == EXIT_FAILURE)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ZeroMQ Error publishing events";
                    std::cout << std::endl;
                }
            }

            const clock_t event_stop = clock();

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Found events: " << found_events << " / " << events_number;
                std::cout << " (" << static_cast<float>(found_events) / events_number * 100 << "%); ";
                std::cout << "Elaboration time: " << (float)(event_stop - event_start) / CLOCKS_PER_SEC * 1000 << " ms; ";
                std::cout << std::endl;
            }

        }

        // Remember to free buffers
        free(topic);
        free(input_buffer);

        result = receive_byte_message(abcd_data_socket, &topic, (void **)(&input_buffer), &size, true, global_status.verbosity);
    }

    return true;
}

/******************************************************************************/
/* Specific actions                                                           */
/******************************************************************************/

state actions::start(status&)
{
    return states::CREATE_CONTEXT;
}

state actions::create_context(status &global_status)
{
    // Creates a ØMQ context
    void *context = zmq_ctx_new();
    if (!context)
    {
        // No errors are defined for this function
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on context creation";
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    global_status.context = context;

    return states::CREATE_SOCKETS;
}

state actions::create_sockets(status &global_status)
{
    void *context = global_status.context;

    // Creates the status socket
    void *status_socket = zmq_socket(context, ZMQ_PUB);
    if (!status_socket)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on status socket creation: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Creates the data socket
    void *data_socket = zmq_socket(context, ZMQ_PUB);
    if (!data_socket)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on data socket creation: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Creates the abcd data socket
    void *abcd_data_socket = zmq_socket(context, ZMQ_SUB);
    if (!abcd_data_socket)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on abcd data socket creation: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    global_status.status_socket = status_socket;
    global_status.data_socket = data_socket;
    global_status.abcd_data_socket = abcd_data_socket;

    return states::BIND_SOCKETS;
}

state actions::bind_sockets(status &global_status)
{
    std::string status_address = global_status.status_address;
    std::string data_address = global_status.data_address;
    std::string abcd_data_address = global_status.abcd_data_address;

    // Binds the status socket to its address
    const int s = zmq_bind(global_status.status_socket, status_address.c_str());
    if (s != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on status socket binding: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Binds the status socket to its address
    const int d = zmq_bind(global_status.data_socket, data_address.c_str());
    if (d != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on status socket binding: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Connects to the abcd data socket to its address
    const int a = zmq_connect(global_status.abcd_data_socket, abcd_data_address.c_str());
    if (a != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on abcd data socket connection: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Subscribe to data topic
    const std::string data_topic("data_abcd_events");
    zmq_setsockopt(global_status.abcd_data_socket, ZMQ_SUBSCRIBE, data_topic.c_str(), data_topic.size());

    return states::READ_CONFIG;
}

state actions::read_config(status &global_status)
{
    const std::string config_file = global_status.config_file;

    if (config_file.length() <= 0)
    {
        return states::ACCUMULATE_PUBLISH_STATUS;
    }

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Reading file: ";
        std::cout << config_file.c_str();
        std::cout << std::endl;
    }

    json_error_t error;

    json_t *new_config = json_load_file(config_file.c_str(), 0, &error);

    if (!new_config)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Parse error while reading config file: ";
        std::cout << error.text;
        std::cout << " (source: ";
        std::cout << error.source;
        std::cout << ", line: ";
        std::cout << error.line;
        std::cout << ", column: ";
        std::cout << error.column;
        std::cout << ", position: ";
        std::cout << error.position;
        std::cout << "); ";
        std::cout << std::endl;

        return states::ACCUMULATE_PUBLISH_STATUS;
    }

    global_status.config = new_config;

    return states::INITIAL_CLEAR_MEMORY;
}

state actions::initial_clear_memory(status &global_status)
{
    actions::generic::clear_memory(global_status);

    return states::APPLY_CONFIG;
}

state actions::apply_config(status &global_status)
{
    if (!global_status.rng)
    {
        gsl_rng_env_setup();
        const gsl_rng_type *T = gsl_rng_default;
        global_status.rng = gsl_rng_alloc(T);
    }

    json_t *config = global_status.config;

    json_t *json_accumulation_time = json_object_get(config, "accumulation_time");

    if (json_accumulation_time != NULL && json_is_number(json_accumulation_time))
    {
        global_status.accumulation_time = json_number_value(json_accumulation_time);
        global_status.expiration_time = (global_status.accumulation_time * 1.1) * 1000000000ULL;
    }

    json_t *json_publish_timeout = json_object_get(config, "publish_timeout");

    if (json_publish_timeout != NULL && json_is_number(json_publish_timeout))
    {
        global_status.publish_timeout = json_number_value(json_publish_timeout);
    }

    json_t *json_publish_fit_events = json_object_get(config, "publish_fit_events");

    if (json_publish_fit_events != NULL && json_is_true(json_publish_fit_events))
    {
        global_status.publish_fit_events = true;
    }
    else if (json_publish_fit_events != NULL && json_is_false(json_publish_fit_events))
    {
        global_status.publish_fit_events = false;
    }
    else
    {
        global_status.publish_fit_events = defaults_califo_publish_fit_events;
    }

    json_t *json_channels = json_object_get(config, "channels");

    if (json_channels != NULL && json_is_array(json_channels))
    {
        size_t index;
        json_t *value;

        json_array_foreach(json_channels, index, value) {
            // This block of code that uses the index and value variables
            // that are set in the json_array_foreach() macro.

            json_t *json_id = json_object_get(value, "id");

            if (json_id != NULL && json_is_integer(json_id)) {
                const unsigned int id = json_integer_value(json_id);

                if (global_status.verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Found channel: " << id << "; ";
                    std::cout << std::endl;
                }

                json_t *json_enabled = json_object_get(value, "enable");

                if (json_is_true(json_enabled))
                {
                    global_status.enabled_channels.insert(id);

                    if (global_status.verbosity > 0)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "Channel: " << id << " detected as enabled; ";
                        std::cout << std::endl;
                    }

                    unsigned int bins = defaults_califo_bins_E;
                    double min = defaults_califo_min_E;
                    double max = defaults_califo_max_E;

                    json_t *json_bins = json_object_get(value, "bins");
                    if (json_bins != NULL && json_is_integer(json_bins)) {
                        bins = json_integer_value(json_bins);
                    }
                    json_t *json_min = json_object_get(value, "min");
                    if (json_min != NULL && json_is_number(json_min)) {
                        min = json_number_value(json_min);
                    }
                    json_t *json_max = json_object_get(value, "max");
                    if (json_max != NULL && json_is_number(json_max)) {
                        max = json_number_value(json_max);
                    }
                
                    global_status.histos_E[id] = histogram_create(bins, min, max, 0);
                    global_status.fifoes[id] = binary_fifo::binary_fifo();
                    global_status.fifoes[id].set_expiration_time(global_status.expiration_time);

                    double peak_position = defaults_califo_peak_position;
                    double peak_width = defaults_califo_peak_width;
                    double peak_height = defaults_califo_peak_height;
                    double peak_alpha = defaults_califo_peak_alpha;
                    double peak_beta = defaults_califo_peak_beta;
                    double peak_tolerance = defaults_califo_peak_tolerance;

                    json_t *json_peak = json_object_get(value, "peak");
                    if (json_peak != NULL && json_is_object(json_peak))
                    {
                        json_t *json_peak_position = json_object_get(json_peak, "position");
                        if (json_peak_position != NULL && json_is_number(json_peak_position)) {
                            peak_position = json_number_value(json_peak_position);
                        }
                
                        json_t *json_peak_width = json_object_get(json_peak, "width");
                        if (json_peak_width != NULL && json_is_number(json_peak_width)) {
                            peak_width = json_number_value(json_peak_width);
                        }
                
                        json_t *json_peak_height = json_object_get(json_peak, "height");
                        if (json_peak_height != NULL && json_is_number(json_peak_height)) {
                            peak_height = json_number_value(json_peak_height);
                        }
                
                        json_t *json_peak_alpha = json_object_get(json_peak, "alpha");
                        if (json_peak_alpha != NULL && json_is_number(json_peak_alpha)) {
                            peak_alpha = json_number_value(json_peak_alpha);
                        }
                
                        json_t *json_peak_beta = json_object_get(json_peak, "beta");
                        if (json_peak_beta != NULL && json_is_number(json_peak_beta)) {
                            peak_beta = json_number_value(json_peak_beta);
                        }
                
                        json_t *json_peak_tolerance = json_object_get(json_peak, "tolerance");
                        if (json_peak_tolerance != NULL && json_is_number(json_peak_tolerance)) {
                            peak_tolerance = json_number_value(json_peak_tolerance);
                        }
                    }
                
                    global_status.peaks[id] = {peak_height, peak_position, peak_width, peak_beta, peak_alpha, 0};
                    global_status.last_fitted_peaks[id] = {peak_height, peak_position, peak_width, peak_beta, peak_alpha, 0};
                    global_status.peak_tolerances[id] = peak_tolerance;
                    global_status.last_scale_factors[id] = 1.0;

                    json_t *json_background = json_object_get(value, "background_estimate");
                    if (json_background != NULL && json_is_object(json_background))
                    {
                        bool background_enable = defaults_califo_background_enable;
                        int background_iterations = defaults_califo_background_iterations;
                        bool background_enable_smooth = defaults_califo_background_enable_smooth;
                        int background_smooth = defaults_califo_background_smooth;
                        int background_order = defaults_califo_background_order;
                        bool background_compton = defaults_califo_background_compton;

                        json_t *json_background_enable = json_object_get(json_background, "enable");
                        if (json_background_enable != NULL && json_is_boolean(json_background_enable)) {
                            background_enable = json_is_true(json_background_enable);
                        }

                        json_t *json_background_iterations = json_object_get(json_background, "background_iterations");
                        if (json_background_iterations != NULL && json_is_integer(json_background_iterations)) {
                            const unsigned int t = json_integer_value(json_background_iterations);

                            if (t >= 1 && bins >= 2 * t + 1)
                            {
                                background_iterations = t;
                            }
                        }

                        json_t *json_background_smooth = json_object_get(json_background, "smooth");
                        if (json_background_smooth != NULL && json_is_integer(json_background_smooth)) {
                            const unsigned int t = json_integer_value(json_background_smooth);

                            if (t == 3) {
                                background_enable_smooth = true;
                                background_smooth = background::kBackSmoothing3;
                            } else if (t == 5) {
                                background_enable_smooth = true;
                                background_smooth = background::kBackSmoothing5;
                            } else if (t == 7) {
                                background_enable_smooth = true;
                                background_smooth = background::kBackSmoothing7;
                            } else if (t == 9) {
                                background_enable_smooth = true;
                                background_smooth = background::kBackSmoothing9;
                            } else if (t == 11) {
                                background_enable_smooth = true;
                                background_smooth = background::kBackSmoothing11;
                            } else if (t == 13) {
                                background_enable_smooth = true;
                                background_smooth = background::kBackSmoothing13;
                            } else if (t == 15) {
                                background_enable_smooth = true;
                                background_smooth = background::kBackSmoothing15;
                            } else {
                                background_enable_smooth = false;
                            }
                        } else {
                            background_enable_smooth = false;
                        }

                        json_t *json_background_order = json_object_get(json_background, "order");
                        if (json_background_order != NULL && json_is_integer(json_background_order)) {
                            const unsigned int t = json_integer_value(json_background_order);

                            if (t == 2) {
                                background_order = background::kBackOrder2;
                            } else if (t == 4) {
                                background_order = background::kBackOrder4;
                            } else if (t == 6) {
                                background_order = background::kBackOrder6;
                            } else if (t == 8) {
                                background_order = background::kBackOrder8;
                            } else {
                                background_order = 2;
                            }
                        } else {
                            background_order = 2;
                        }

                        json_t *json_background_compton = json_object_get(json_background, "compton");
                        if (json_background_compton != NULL && json_is_boolean(json_background_compton)) {
                            background_compton = json_is_true(json_background_compton);
                        }

                        global_status.background_estimates[id] = {background_enable,
                                                         background_iterations,
                                                         background::kBackIncreasingWindow,
                                                         background_enable_smooth,
                                                         background_smooth,
                                                         background_order,
                                                         background_compton};
                    }
                    else
                    {
                        global_status.background_estimates[id] = {false, 0, 0, false, 0, 0, false};
                    }

                    const std::string event_message = "Configuration of histogram for channel: " + std::to_string(id);

                    json_t *json_event_message = json_object();
                    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
                    json_object_set_new_nocheck(json_event_message, "event", json_string(event_message.c_str()));

                    actions::generic::publish_message(global_status, defaults_califo_events_topic, json_event_message);

                    json_decref(json_event_message);
                }
            }
        }
    }

    std::chrono::time_point<std::chrono::system_clock> system_start = std::chrono::system_clock::now();
    global_status.system_start = system_start;

    return states::ACCUMULATE_PUBLISH_STATUS;
}

state actions::accumulate_publish_status(status &global_status)
{
    actions::generic::publish_status(global_status);

    std::chrono::time_point<std::chrono::system_clock> last_publication = std::chrono::system_clock::now();
    global_status.last_publication = last_publication;

    return states::ACCUMULATE_READ_SOCKET;
}

state actions::accumulate_read_socket(status &global_status)
{
    actions::generic::read_socket(global_status);

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    if (now - global_status.last_publication > std::chrono::seconds(global_status.publish_timeout))
    {
        return states::ACCUMULATE_PUBLISH_STATUS;
    }
    else if (now - global_status.system_start > std::chrono::seconds(global_status.accumulation_time))
    {
        return states::NORMAL_FIT_PEAK;
    }

    return states::ACCUMULATE_READ_SOCKET;
}

state actions::normal_publish_status(status &global_status)
{
    actions::generic::publish_status(global_status);

    std::chrono::time_point<std::chrono::system_clock> last_publication = std::chrono::system_clock::now();
    global_status.last_publication = last_publication;

    return states::NORMAL_READ_SOCKET;
}

state actions::normal_fit_peak(status &global_status)
{
    const clock_t fit_start = clock();

    // Retrieving all the histograms from the fifoes
    for (auto &pair: global_status.histos_E)
    {
        const unsigned int channel = pair.first;
        histogram_t *const histo_E = pair.second;

        if (histo_E != NULL)
        {
            json_t *json_fit_event_message;

            if (global_status.publish_fit_events) {
                const std::string event_message = "Fit on channel " + std::to_string(channel);

                json_fit_event_message = json_object();

                json_object_set_new_nocheck(json_fit_event_message,
                                            "type",
                                            json_string("event"));
                json_object_set_new_nocheck(json_fit_event_message,
                                            "event",
                                            json_string(event_message.c_str()));
                json_object_set_new_nocheck(json_fit_event_message,
                                            "channel",
                                            json_integer(channel));
            }

            global_status.fifoes[channel].update();
            histogram_reset(histo_E);

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Retrieving data for channel: " << channel << "; ";
                std::cout << std::endl;
            }

            const binary_fifo::time_point to_time = std::chrono::system_clock::now();
            const binary_fifo::time_point from_time = std::chrono::system_clock::now() - std::chrono::seconds(global_status.accumulation_time);
 
            std::vector<binary_fifo::binary_data> data =
                global_status.fifoes[channel].get_data(from_time, to_time);

            // Creating an empty istogram as a temporary hook for the histogram_add_to() function
            histogram_t temp_histo = {0,
                                      histo_E->bins,
                                      histo_E->min,
                                      histo_E->max,
                                      histo_E->bin_width,
                                      nullptr};
 
            for (auto &this_data: data)
            {
                temp_histo.histo = reinterpret_cast<counter_type*>(this_data.data());
                
                histogram_add_to(histo_E, &temp_histo);
            }

            if (global_status.publish_fit_events) {
                json_object_set_new_nocheck(json_fit_event_message,
                                            "raw_data",
                                            histogram_to_json(histo_E));
            }

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Fitting data for channel: " << channel << "; ";
                std::cout << std::endl;
            }

            struct background_parameters background_estimate = global_status.background_estimates[channel];

            if (background_estimate.enable) {
                const size_t histo_size = histo_E->bins * sizeof(counter_type) / sizeof(uint8_t);

                std::vector<double> y_values(histo_E->bins);
                memcpy(y_values.data(), histo_E->histo, histo_size * sizeof(uint8_t));

                std::vector<double> y_background =
                        background::background( y_values,
                                                background_estimate.iterations,
                                                background_estimate.window_direction,
                                                background_estimate.order,
                                                background_estimate.enable_smooth,
                                                background_estimate.smooth,
                                                background_estimate.compton);
                
                temp_histo.histo = reinterpret_cast<counter_type*>(y_background.data());

                histogram_subtract_from(histo_E, &temp_histo);

                if (global_status.publish_fit_events) {
                    json_object_set_new_nocheck(json_fit_event_message,
                                                "background",
                                                histogram_to_json(&temp_histo));
                    json_object_set_new_nocheck(json_fit_event_message,
                                                "data",
                                                histogram_to_json(histo_E));
                }
            }

            struct peak_parameters initial = global_status.peaks[channel];

            struct peak_parameters p = fit_peak(histo_E, initial);

            const double peak_position = global_status.peaks[channel].mu;
            const double last_peak_position = global_status.last_fitted_peaks[channel].mu;
            const double new_peak_position = p.mu;
            const double peak_width = global_status.peaks[channel].sigma;
            const double last_peak_width = global_status.last_fitted_peaks[channel].sigma;
            const double new_peak_width = p.sigma;
            const double peak_distance = fabs(new_peak_position - last_peak_position);
            const double peak_tolerance = global_status.peak_tolerances[channel];
            const double last_scale_factor = global_status.last_scale_factors[channel];
            const double new_scale_factor = peak_position / new_peak_position;

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Peak position: " << peak_position << "; ";
                std::cout << "Last: " << last_peak_position << "; ";
                std::cout << "New: " << new_peak_position << "; ";
                std::cout << "Distance: " << peak_distance << "; ";
                std::cout << "Tolerance: " << peak_tolerance << "; ";
                std::cout << "Last scale factor: " << last_scale_factor << "; ";
                std::cout << "New: " << new_scale_factor << "; ";
                std::cout << "Peak width: " << peak_width << "; ";
                std::cout << "Last: " << last_peak_width << "; ";
                std::cout << "New: " << new_peak_width << "; ";
                std::cout << std::endl;
            }

            if (global_status.publish_fit_events) {
                json_t *peak_array = json_array();

                const unsigned int bins = histo_E->bins;
                const data_type min = histo_E->min;
                const data_type bin_width = histo_E->bin_width;

                for (unsigned int i = 0; i < bins; i++) {
                    const double t = min + bin_width * i;
                    const double val = peak(p.A, p.mu, p.sigma, p.B, p.alpha, t);

                    json_array_append_new(peak_array, json_real(val));
                }

                json_object_set_new_nocheck(json_fit_event_message,
                                            "fit",
                                            peak_array);
            }
            

            if (peak_distance < peak_tolerance)
            {
                global_status.last_fitted_peaks[channel] = p;
                global_status.last_scale_factors[channel] = new_scale_factor;
            }
            else
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "ERROR: Peak is too far from last fit!!!; ";
                std::cout << std::endl;

                const std::string event_message = "Peak too far";

                json_t *json_event_message = json_object();

                json_object_set_new_nocheck(json_event_message,
                                            "type",
                                            json_string("error"));
                json_object_set_new_nocheck(json_event_message,
                                            "error",
                                            json_string(event_message.c_str()));

                actions::generic::publish_message(global_status,
                                                  defaults_califo_events_topic,
                                                  json_event_message);

                json_decref(json_event_message);
            }
            if (global_status.publish_fit_events) {
                actions::generic::publish_message(global_status,
                                                  defaults_califo_events_topic,
                                                  json_fit_event_message);

                json_decref(json_fit_event_message);
            }
        }
    }

    const clock_t fit_stop = clock();

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Elaboration time: " << (float)(fit_stop - fit_start) / CLOCKS_PER_SEC * 1000 << " ms; ";
        std::cout << std::endl;
    }

    return states::NORMAL_PUBLISH_STATUS;
}

state actions::normal_read_socket(status &global_status)
{
    actions::generic::read_socket(global_status);

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    if (now - global_status.last_publication > std::chrono::seconds(global_status.publish_timeout))
    {
        return states::NORMAL_FIT_PEAK;
    }

    return states::NORMAL_READ_SOCKET;
}

state actions::close_sockets(status &global_status)
{
    void *status_socket = global_status.status_socket;
    void *data_socket = global_status.data_socket;
    void *abcd_data_socket = global_status.abcd_data_socket;

    const int s = zmq_close(status_socket);
    if (s != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on status socket close: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    const int d = zmq_close(data_socket);
    if (d != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on data socket close: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    const int a = zmq_close(abcd_data_socket);
    if (a != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on abcd data socket close: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    return states::DESTROY_CONTEXT;
}

state actions::destroy_context(status &global_status)
{
    void *context = global_status.context;

    const int c = zmq_ctx_destroy(context);
    if (c != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on context destroy: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    return states::STOP;
}

state actions::communication_error(status &global_status)
{
    const std::string event_message = "Communication error";

    json_t *json_event_message = json_object();
    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string(event_message.c_str()));

    actions::generic::publish_message(global_status, defaults_califo_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::CLOSE_SOCKETS;
}

state actions::stop(status&)
{
    return states::STOP;
}
