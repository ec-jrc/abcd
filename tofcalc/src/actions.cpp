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
#include <limits>

extern "C" {
#include <zmq.h>
#include <jansson.h>
}

#include "typedefs.hpp"
#include "states.hpp"
#include "events.hpp"
#include "actions.hpp"

#define counter_type uint64_t

extern "C" {
#include "utilities_functions.h"
#include "socket_functions.h"
#include "jansson_socket_functions.h"
#include "histogram.h"
#include "histogram2D.h"
}

#define BUFFER_SIZE 32

/******************************************************************************/
/* Quicksort utilities                                                        */
/******************************************************************************/

intmax_t partition(event_PSD *events, intmax_t low, intmax_t high)
{
    const event_PSD pivot = events[high];

    intmax_t i = low - 1;

    for (intmax_t j = low; j <= high - 1; j++)
    {
        //std::cout << "low: " << low << "; high: " << high << "; i: " << i << "; j: " << j << std::endl;

        if (events[j].timestamp <= pivot.timestamp)
        {
            i += 1;

            const event_PSD temp = events[j];
            events[j] = events[i];
            events[i] = temp;
        }
    }

    //std::cout << "low: " << low << "; high: " << high << "; i: " << i << std::endl;

    const event_PSD temp = events[high];
    events[high] = events[i + 1];
    events[i + 1] = temp;

    return i + 1;
}

void quicksort(event_PSD *events, intmax_t low, intmax_t high)
{
    //std::cout << "low: " << low << "; high: " << high << std::endl;

    if (low < high)
    {
        const intmax_t pivot_index = partition(events, low, high);

        //std::cout << "low: " << low << "; high: " << high << "; pivot_index: " << pivot_index << std::endl;

        quicksort(events, low, pivot_index - 1);
        quicksort(events, pivot_index + 1, high);
    }
}

/******************************************************************************/
/* Generic actions                                                            */
/******************************************************************************/

void actions::generic::publish_message(status &global_status,
                                      std::string topic,
                                      json_t *status_message)
{
    void *status_socket = global_status.status_socket;
    const unsigned long int status_msg_ID = global_status.status_msg_ID;

    char time_buffer[BUFFER_SIZE];
    time_string(time_buffer, BUFFER_SIZE, NULL);

    json_object_set_new(status_message, "module", json_string("tofcalc"));
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

    json_t *reference_channels = json_array();
    if (reference_channels == NULL)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to create reference_channels json; ";
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
        json_decref(reference_channels);

        // I am not sure what to do here...
        return false;
    }

    json_t *channels_configs = json_array();
    if (channels_configs == NULL)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to create channels_configs json; ";
        std::cout << std::endl;

        json_decref(status_message);
        json_decref(reference_channels);
        json_decref(active_channels);

        // I am not sure what to do here...
        return false;
    }

    json_t *channels_statuses = json_array();
    if (channels_statuses == NULL)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to create channels_statuses json; ";
        std::cout << std::endl;

        json_decref(status_message);
        json_decref(reference_channels);
        json_decref(active_channels);
        json_decref(channels_configs);

        // I am not sure what to do here...
        return false;
    }


    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Reference channels: ";
        for (const unsigned int &channel: global_status.reference_channels)
        {
            std::cout << channel << " ";
        }
        std::cout << std::endl;

        std::cout << '[' << time_buffer << "] ";
        std::cout << "Active channels: ";
        for (const unsigned int &channel: global_status.active_channels)
        {
            std::cout << channel << " ";
        }
        std::cout << std::endl;
    }

    const auto now = std::chrono::system_clock::now();
    const auto pub_delta_time = std::chrono::duration_cast<std::chrono::duration<long int>>(now - global_status.last_publication);
    const double pubtime = static_cast<double>(pub_delta_time.count());

    for (const unsigned int &channel: global_status.reference_channels)
    {
        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Publishing status for reference channel: " << channel << "; ";
            std::cout << std::endl;
        }

        json_array_append_new(reference_channels, json_integer(channel));

        json_t *channel_config = json_object();

        json_object_set_new_nocheck(channel_config, "id", json_integer(channel));
        json_object_set_new_nocheck(channel_config, "enabled", json_true());
        json_object_set_new_nocheck(channel_config, "reference", json_true());

        json_array_append_new(channels_configs, channel_config);
    }

    for (const unsigned int &channel: global_status.active_channels)
    {
        histogram_t *const histo_ToF = global_status.histos_ToF[channel];
        histogram_t *const histo_E = global_status.histos_E[channel];
        histogram2D_t *const histo_EToF = global_status.histos_EToF[channel];
        const unsigned int channel_counts = global_status.partial_counts[channel];
        const double channel_rate = channel_counts / pubtime;

        if (histo_ToF && histo_E && histo_EToF) {
            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Publishing status for active channel: " << channel << "; ";
                std::cout << std::endl;
            }

            json_t *histo_ToF_config = histogram_config_to_json(histo_ToF);
            json_t *histo_E_config = histogram_config_to_json(histo_E);
            json_t *histo_EToF_config = histogram2D_config_to_json(histo_EToF);

            json_t *channel_config = json_object();

            json_object_set_new_nocheck(channel_config, "id", json_integer(channel));
            json_object_set_new_nocheck(channel_config, "enabled", json_true());
            json_object_set_new_nocheck(channel_config, "reference", json_false());
            json_object_set_new_nocheck(channel_config, "ToF", histo_ToF_config);
            json_object_set_new_nocheck(channel_config, "energy", histo_E_config);
            json_object_set_new_nocheck(channel_config, "EToF", histo_EToF_config);
            json_array_append_new(channels_configs, channel_config);

            json_t *channel_status = json_object();

            json_object_set_new_nocheck(channel_status, "id", json_integer(channel));
            json_object_set_new_nocheck(channel_status, "enabled", json_true());
            json_object_set_new_nocheck(channel_status, "rate", json_real(channel_rate));
            json_array_append_new(channels_statuses, channel_status);

            json_array_append_new(active_channels, json_integer(channel));
        }
    }

    json_object_set_new_nocheck(status_message, "configs", channels_configs);
    json_object_set_new_nocheck(status_message, "statuses", channels_statuses);
    json_object_set_new_nocheck(status_message, "reference_channels", reference_channels);
    json_object_set_new_nocheck(status_message, "active_channels", active_channels);

    actions::generic::publish_message(global_status, defaults_tofcalc_status_topic, status_message);

    json_decref(status_message);

    // Resetting counts
    for (auto &pair: global_status.partial_counts)
    {
        // FIXME: Check if this works
        pair.second = 0;
    }

    return true;
}

bool actions::generic::publish_data(status &global_status)
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

    json_t *reference_channels = json_array();
    if (reference_channels == NULL)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to create reference_channels json; ";
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
        json_decref(reference_channels);

        // I am not sure what to do here...
        return false;
    }

    json_t *channels_configs = json_array();
    if (channels_configs == NULL)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to create channels_configs json; ";
        std::cout << std::endl;

        json_decref(status_message);
        json_decref(reference_channels);
        json_decref(active_channels);

        // I am not sure what to do here...
        return false;
    }

    const auto now = std::chrono::system_clock::now();
    const auto pub_delta_time = std::chrono::duration_cast<std::chrono::duration<long int>>(now - global_status.last_publication);
    const double pubtime = static_cast<double>(pub_delta_time.count());

    for (const unsigned int &channel: global_status.reference_channels)
    {
        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Publishing data for reference channel: " << channel << "; ";
            std::cout << std::endl;
        }

        json_array_append_new(reference_channels, json_integer(channel));

        json_t *channel_config = json_object();

        json_object_set_new_nocheck(channel_config, "id", json_integer(channel));
        json_object_set_new_nocheck(channel_config, "enabled", json_true());
        json_object_set_new_nocheck(channel_config, "reference", json_true());

        json_array_append_new(channels_configs, channel_config);
    }

    for (const unsigned int &channel: global_status.active_channels)
    {
        histogram_t *const histo_ToF = global_status.histos_ToF[channel];
        histogram_t *const histo_E = global_status.histos_E[channel];
        histogram2D_t *const histo_EToF = global_status.histos_EToF[channel];
        const unsigned int channel_counts = global_status.partial_counts[channel];
        const double channel_rate = channel_counts / pubtime;

        if (histo_ToF && histo_E && histo_EToF) {
            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Publishing data for active channel: " << channel << "; ";
                std::cout << std::endl;
            }

            json_t *histo_ToF_config = histogram_to_json(histo_ToF);
            json_t *histo_E_config = histogram_to_json(histo_E);
            json_t *histo_EToF_config = histogram2D_to_json(histo_EToF);

            json_t *channel_config = json_object();

            json_object_set_new_nocheck(channel_config, "id", json_integer(channel));
            json_object_set_new_nocheck(channel_config, "enabled", json_true());
            json_object_set_new_nocheck(channel_config, "reference", json_false());
            json_object_set_new_nocheck(channel_config, "rate", json_real(channel_rate));
            json_object_set_new_nocheck(channel_config, "ToF", histo_ToF_config);
            json_object_set_new_nocheck(channel_config, "energy", histo_E_config);
            json_object_set_new_nocheck(channel_config, "EToF", histo_EToF_config);

            json_array_append_new(channels_configs, channel_config);

            json_array_append_new(active_channels, json_integer(channel));
        }
    }

    json_object_set_new_nocheck(status_message, "data", channels_configs);
    json_object_set_new_nocheck(status_message, "reference_channels", reference_channels);
    json_object_set_new_nocheck(status_message, "active_channels", active_channels);

    char time_buffer[BUFFER_SIZE];
    time_string(time_buffer, BUFFER_SIZE, NULL);

    json_object_set_new_nocheck(status_message, "module", json_string("tofcalc"));
    json_object_set_new_nocheck(status_message, "timestamp", json_string(time_buffer));
    json_object_set_new_nocheck(status_message, "msg_ID", json_integer(global_status.data_msg_ID));

    send_json_message(global_status.data_socket, const_cast<char*>(defaults_tofcalc_data_histograms_topic), status_message, 0);

    global_status.data_msg_ID += 1;

    json_decref(status_message);

    return true;
}

bool actions::generic::read_socket(status &global_status)
{
    double max_ToF = std::numeric_limits<double>::min();
    double min_ToF = std::numeric_limits<double>::max();

    // Determining the global ToF maximum and minimum
    for (auto &pair: global_status.histos_ToF)
    {
        const unsigned int channel = pair.first;
        histogram_t *const histo_ToF = pair.second;

        if (histo_ToF != NULL) {
            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Channel: " << channel << "; ";
                std::cout << "ToF max: " << histo_ToF->max << "; ";
                std::cout << "ToF min: " << histo_ToF->min << "; ";
                std::cout << std::endl;
            }

            if (histo_ToF->max > max_ToF) {
                max_ToF = histo_ToF->max;
            }
            if (histo_ToF->min < min_ToF) {
                min_ToF = histo_ToF->min;
            }
        }
    }

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ToF max: " << max_ToF << "; ";
        std::cout << "ToF min: " << min_ToF << "; ";
        std::cout << std::endl;
    }

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
            const double ns_per_sample = global_status.ns_per_sample;

            const clock_t event_start = clock();

            const size_t data_size = size;

            const size_t events_number = data_size / sizeof(event_PSD);

            size_t found_coincidences = 0;

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

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Sorting buffer... ";
                std::cout << std::endl;
            }

            quicksort(events, 0, events_number - 1);

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Sorted buffer! ";
                std::cout << std::endl;
            }

            for (size_t i = 0; i < events_number; i++)
            {
                //std::cout << "i: " << i << "; Events number: " << events_number << "; " << std::endl;
                const event_PSD this_event = events[i];

                // If it is a reference channel we can look for the coincidences
                if (std::find(global_status.reference_channels.begin(),
                              global_status.reference_channels.end(),
                              this_event.channel) != global_status.reference_channels.end())
                {
                    //std::cout << "Reference channel " << std::endl;

                    for (intmax_t j = (i + 1); j < static_cast<int64_t>(events_number); j++)
                    {
                        //std::cout << "Forward:  i: " << i << "; j: " << j << std::endl;
                        const event_PSD that_event = events[j];
                        
                        const double time_of_flight = (static_cast<int64_t>(that_event.timestamp)
                                                       -
                                                       static_cast<int64_t>(this_event.timestamp))
                                                       * ns_per_sample;
    
                        if (global_status.verbosity > 1)
                        {
                            char time_buffer[BUFFER_SIZE];
                            time_string(time_buffer, BUFFER_SIZE, NULL);
                            std::cout << '[' << time_buffer << "] ";
                            std::cout << "Forward: ";
                            std::cout << "i: " << i << "; ";
                            std::cout << "ch: " << (int)this_event.channel << "; ";
                            std::cout << "j: " << j << "; ";
                            std::cout << "ch: " << (int)that_event.channel << "; ";
                            std::cout << "time_of_flight: " << time_of_flight << "; ";
                            std::cout << "ToF interval: [" << min_ToF << ", " << max_ToF << "]; ";
                            std::cout << std::endl;
                        }

                        // We first look for the time window as it is the strongest requirement
                        if (min_ToF <= time_of_flight && time_of_flight < max_ToF)
                        {
                            // If it is not a reference channel we can tag this as a coincidence
                            if (std::find(global_status.reference_channels.begin(),
                                          global_status.reference_channels.end(),
                                          that_event.channel) == global_status.reference_channels.end())
                            {
                                if (global_status.verbosity > 1)
                                {
                                    char time_buffer[BUFFER_SIZE];
                                    time_string(time_buffer, BUFFER_SIZE, NULL);
                                    std::cout << '[' << time_buffer << "] ";
                                    std::cout << "Forward coincidence!; ";
                                    std::cout << "i: " << i << "; ";
                                    std::cout << "channel: " << (unsigned int)this_event.channel << "; ";
                                    std::cout << "j: " << j << "; ";
                                    std::cout << "channel: " << (unsigned int)that_event.channel << "; ";
                                    std::cout << "ToF: " << time_of_flight << "; ";
                                    std::cout << std::endl;
                                }

                                histogram_t *const histo_ToF = global_status.histos_ToF[that_event.channel];
                                histogram_t *const histo_E = global_status.histos_E[that_event.channel];
                                histogram2D_t *const histo_EToF = global_status.histos_EToF[that_event.channel];

                                // This is not really an error as a user might not be interested on
                                // the ToF of this channel
                                if (!histo_ToF || !histo_E || !histo_EToF) {
                                    if (global_status.verbosity > 1) {
                                        char time_buffer[BUFFER_SIZE];
                                        time_string(time_buffer, BUFFER_SIZE, NULL);
                                        std::cout << '[' << time_buffer << "] ";
                                        std::cout << "WARNING: Unable to get histo for channel: " << that_event.channel << "; ";
                                        std::cout << std::endl;
                                    }
                                }
                                else
                                {
                                    if (histo_EToF->min_x <= time_of_flight && time_of_flight < histo_EToF->max_x &&
                                        histo_EToF->min_y <= that_event.qlong && that_event.qlong < histo_EToF->max_y) {
                                        histogram_fill(histo_ToF, time_of_flight);
                                        histogram_fill(histo_E, that_event.qlong);
                                        histogram2D_fill(histo_EToF, time_of_flight, that_event.qlong);
                                        global_status.partial_counts[that_event.channel] += 1;
    
                                        found_coincidences += 1;
                                    }
                                }
                            }
                        }
                        else
                        {
                            break;
                        }
                    }

                    for (intmax_t j = (i - 1); j > 0; j--)
                    {
                        //std::cout << "Backward:  i: " << i << "; j: " << j << std::endl;
                        const event_PSD that_event = events[j];
                        const double time_of_flight = (static_cast<int64_t>(that_event.timestamp)
                                                       -
                                                       static_cast<int64_t>(this_event.timestamp))
                                                       * ns_per_sample;
    
                        if (global_status.verbosity > 1)
                        {
                            char time_buffer[BUFFER_SIZE];
                            time_string(time_buffer, BUFFER_SIZE, NULL);
                            std::cout << '[' << time_buffer << "] ";
                            std::cout << "Backward: ";
                            std::cout << "i: " << i << "; ";
                            std::cout << "ch: " << (int)this_event.channel << "; ";
                            std::cout << "j: " << j << "; ";
                            std::cout << "ch: " << (int)that_event.channel << "; ";
                            std::cout << "time_of_flight: " << time_of_flight << "; ";
                            std::cout << "ToF interval: [" << min_ToF << ", " << max_ToF << "]; ";
                            std::cout << std::endl;
                        }

                        // We first look for the time window as it is the strongest requirement
                        if (min_ToF <= time_of_flight && time_of_flight < max_ToF)
                        {
    
                            // If it is not a reference channel we can tag this as a coincidence
                            if (std::find(global_status.reference_channels.begin(),
                                          global_status.reference_channels.end(),
                                          that_event.channel) == global_status.reference_channels.end())
                            {
                                if (global_status.verbosity > 1)
                                {
                                    char time_buffer[BUFFER_SIZE];
                                    time_string(time_buffer, BUFFER_SIZE, NULL);
                                    std::cout << '[' << time_buffer << "] ";
                                    std::cout << "Backward coincidence!; ";
                                    std::cout << "i: " << i << "; ";
                                    std::cout << "channel: " << (unsigned int)this_event.channel << "; ";
                                    std::cout << "j: " << j << "; ";
                                    std::cout << "channel: " << (unsigned int)that_event.channel << "; ";
                                    std::cout << "ToF: " << time_of_flight << "; ";
                                    std::cout << std::endl;
                                }

                                histogram_t *const histo_ToF = global_status.histos_ToF[that_event.channel];
                                histogram_t *const histo_E = global_status.histos_E[that_event.channel];
                                histogram2D_t *const histo_EToF = global_status.histos_EToF[that_event.channel];
    
                                // This is not really an error as a user might not be interested on
                                // the ToF of this channel
                                if (!histo_ToF || !histo_E || !histo_EToF) {
                                    if (global_status.verbosity > 1) {
                                        char time_buffer[BUFFER_SIZE];
                                        time_string(time_buffer, BUFFER_SIZE, NULL);
                                        std::cout << '[' << time_buffer << "] ";
                                        std::cout << "WARNING: Unable to get histo for channel: " << this_event.channel << "; ";
                                        std::cout << std::endl;
                                    }
                                }
                                else
                                {
                                    if (histo_EToF->min_x <= time_of_flight && time_of_flight < histo_EToF->max_x &&
                                        histo_EToF->min_y <= that_event.qlong && that_event.qlong < histo_EToF->max_y) {
                                        histogram_fill(histo_ToF, time_of_flight);
                                        histogram_fill(histo_E, that_event.qlong);
                                        histogram2D_fill(histo_EToF, time_of_flight, that_event.qlong);
                                        global_status.partial_counts[that_event.channel] += 1;
    
                                        found_coincidences += 1;
                                    }
                                }
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }

            const clock_t event_stop = clock();

            if (global_status.verbosity > 0)
            {
                const float elaboration_time = (float)(event_stop - event_start) / CLOCKS_PER_SEC * 1000;
                const float elaboration_speed = data_size / elaboration_time * 1000.0 / 1024.0 / 1024.0;
                const float elaboration_rate = events_number / elaboration_time * 1000.0;


                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Found coincidences: " << found_coincidences << " / " << events_number;
                std::cout << " (" << static_cast<float>(found_coincidences) / events_number * 100 << "%); ";
                std::cout << "Elaboration time: " << elaboration_time << " ms; ";
                std::cout << "Elaboration speed: " << elaboration_speed << " MBi/s, ";
                std::cout << elaboration_rate << " evts/s; ";
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

    // Creates the commands socket
    void *commands_socket = zmq_socket(context, ZMQ_PULL);
    if (!commands_socket)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on commands socket creation: ";
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
    global_status.commands_socket = commands_socket;
    global_status.abcd_data_socket = abcd_data_socket;

    return states::BIND_SOCKETS;
}

state actions::bind_sockets(status &global_status)
{
    std::string status_address = global_status.status_address;
    std::string data_address = global_status.data_address;
    std::string commands_address = global_status.commands_address;
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

    // Binds the data socket to its address
    const int d = zmq_bind(global_status.data_socket, data_address.c_str());
    if (d != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on data socket binding: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Binds the commands socket to its address
    const int c = zmq_bind(global_status.commands_socket, commands_address.c_str());
    if (c != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on commands socket binding: ";
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
        return states::PUBLISH_STATUS;
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

        return states::PUBLISH_STATUS;
    }

    global_status.config = new_config;

    return states::APPLY_CONFIG;
}

state actions::apply_config(status &global_status)
{
    const json_t *config = global_status.config;

    json_t *json_channels = json_object_get(config, "channels");

    if (json_channels != NULL && json_is_array(json_channels))
    {
        global_status.reference_channels.clear();
        global_status.active_channels.clear();

        for (auto &pair: global_status.histos_ToF)
        {
            histogram_t *const histo_ToF = pair.second;

            if (histo_ToF != NULL)
            {
                histogram_destroy(histo_ToF);
            }
        }
        global_status.histos_ToF.clear();

        for (auto &pair: global_status.histos_E)
        {
            histogram_t *const histo_E = pair.second;

            if (histo_E != NULL)
            {
                histogram_destroy(histo_E);
            }
        }
        global_status.histos_E.clear();

        for (auto &pair: global_status.histos_EToF)
        {
            histogram2D_t *const histo_EToF = pair.second;

            if (histo_EToF != NULL)
            {
                histogram2D_destroy(histo_EToF);
            }
        }
        global_status.histos_EToF.clear();

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

                json_t *json_reference = json_object_get(value, "reference");

                // We create a ToF histogram only if the channel is not a reference channel
                // because we use reference channels as start pulses and we do not want to
                // merge the ToF of all the other channels in one histogram.
                if (json_is_true(json_reference))
                {
                    global_status.reference_channels.insert(id);

                    if (global_status.verbosity > 0)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "Channel: " << id << " detected as reference; ";
                        std::cout << std::endl;
                    }
                }
                else
                {
                    global_status.active_channels.insert(id);

                    unsigned int bins_ToF = defaults_tofcalc_bins_ToF;
                    double min_ToF = defaults_tofcalc_min_ToF;
                    double max_ToF = defaults_tofcalc_max_ToF;

                    json_t *json_bins_ToF = json_object_get(value, "bins_ToF");
                    if (json_bins_ToF != NULL && json_is_integer(json_bins_ToF)) {
                        bins_ToF = json_integer_value(json_bins_ToF);
                    }
                    json_t *json_min_ToF = json_object_get(value, "min_ToF");
                    if (json_min_ToF != NULL && json_is_number(json_min_ToF)) {
                        min_ToF = json_number_value(json_min_ToF);
                    }
                    json_t *json_max_ToF = json_object_get(value, "max_ToF");
                    if (json_max_ToF != NULL && json_is_number(json_max_ToF)) {
                        max_ToF = json_number_value(json_max_ToF);
                    }
                
                    unsigned int bins_E = defaults_tofcalc_bins_E;
                    double min_E = defaults_tofcalc_min_E;
                    double max_E = defaults_tofcalc_max_E;

                    json_t *json_bins_E = json_object_get(value, "bins_E");
                    if (json_bins_E != NULL && json_is_integer(json_bins_E)) {
                        bins_E = json_integer_value(json_bins_E);
                    }
                    json_t *json_min_E = json_object_get(value, "min_E");
                    if (json_min_E != NULL && json_is_number(json_min_E)) {
                        min_E = json_number_value(json_min_E);
                    }
                    json_t *json_max_E = json_object_get(value, "max_E");
                    if (json_max_E != NULL && json_is_number(json_max_E)) {
                        max_E = json_number_value(json_max_E);
                    }
                
                    global_status.histos_ToF[id] = histogram_create(bins_ToF,
                                                                    min_ToF,
                                                                    max_ToF,
                                                                    global_status.verbosity);

                    global_status.histos_E[id] = histogram_create(bins_E,
                                                                  min_E,
                                                                  max_E,
                                                                  global_status.verbosity);

                    global_status.histos_EToF[id] = histogram2D_create(bins_ToF,
                                                                       min_ToF,
                                                                       max_ToF,
                                                                       bins_E,
                                                                       min_E,
                                                                       max_E,
                                                                       global_status.verbosity);

                    global_status.partial_counts[id] = 0;

                    const std::string event_message = "Configuration of histogram for channel: " + std::to_string(id);

                    json_t *json_event_message = json_object();
                    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
                    json_object_set_new_nocheck(json_event_message, "event", json_string(event_message.c_str()));

                    actions::generic::publish_message(global_status, defaults_tofcalc_events_topic, json_event_message);

                    json_decref(json_event_message);
                }
            }
        }
    }

    return states::PUBLISH_STATUS;
}

state actions::publish_status(status &global_status)
{
    actions::generic::publish_status(global_status);

    std::chrono::time_point<std::chrono::system_clock> last_publication = std::chrono::system_clock::now();
    global_status.last_publication = last_publication;

    return states::RECEIVE_COMMANDS;
}

state actions::receive_commands(status &global_status)
{
    void *commands_socket = global_status.commands_socket;

    char *buffer;
    size_t size;

    const int result = receive_byte_message(commands_socket, nullptr, (void**)(&buffer), &size, 0, global_status.verbosity);

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "result: " << result << "; ";
        std::cout << "size: " << size << "; ";
        std::cout << std::endl;
    }

    if (size > 0 && result == EXIT_SUCCESS)
    {
        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Message buffer: ";
            std::cout << (char*)buffer;
            std::cout << "; ";
            std::cout << std::endl;
        }

        json_error_t error;
        json_t *json_message = json_loadb(buffer, size, 0, &error);

        free(buffer);

        if (!json_message)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR: ";
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
        }
        else
        {
            json_t *json_command = json_object_get(json_message, "command");
            json_t *json_arguments = json_object_get(json_message, "arguments");

            if (json_command != NULL && json_is_string(json_command))
            {
                const std::string command = json_string_value(json_command);

                if (command == std::string("reset") && json_arguments != NULL)
                {
                    json_t *json_channel = json_object_get(json_arguments, "channel");

                    if (json_channel != NULL && json_is_string(json_channel))
                    {
                        const std::string channel = json_string_value(json_channel);

                        const std::string event_message = "Reset of channel: " + channel;

                        json_t *json_event_message = json_object();
                        json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
                        json_object_set_new_nocheck(json_event_message, "event", json_string(event_message.c_str()));

                        if (global_status.verbosity > 0)
                        {
                            char time_buffer[BUFFER_SIZE];
                            time_string(time_buffer, BUFFER_SIZE, NULL);
                            std::cout << '[' << time_buffer << "] ";
                            std::cout << "Received command: " << command << "; ";
                            std::cout << "Channel: " << channel << "; ";
                            std::cout << std::endl;
                        }

                        if (channel == std::string("all"))
                        {
                            for (auto &pair: global_status.histos_ToF)
                            {
                                histogram_t *const histo_ToF = pair.second;

                                if (histo_ToF != NULL)
                                {
                                    histogram_reset(histo_ToF);
                                }
                            }

                            for (auto &pair: global_status.histos_E)
                            {
                                histogram_t *const histo_E = pair.second;

                                if (histo_E != NULL)
                                {
                                    histogram_reset(histo_E);
                                }
                            }

                            for (auto &pair: global_status.histos_EToF)
                            {
                                histogram2D_t *const histo_EToF = pair.second;

                                if (histo_EToF != NULL)
                                {
                                    histogram2D_reset(histo_EToF);
                                }
                            }

                            for (auto &pair: global_status.partial_counts)
                            {
                                // FIXME: Check if this works
                                pair.second = 0;
                            }

                            actions::generic::publish_message(global_status, defaults_tofcalc_events_topic, json_event_message);
                        }

                        json_decref(json_event_message);
                    }
                    else if (json_channel != NULL && json_is_integer(json_channel))
                    {
                        const unsigned int channel = json_integer_value(json_channel);

                        std::string event_message = "Reset of channel: " + std::to_string(channel);

                        json_t *json_event_message = json_object();
                        json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
                        json_object_set_new_nocheck(json_event_message, "event", json_string(event_message.c_str()));

                        if (global_status.verbosity > 0)
                        {
                            char time_buffer[BUFFER_SIZE];
                            time_string(time_buffer, BUFFER_SIZE, NULL);
                            std::cout << '[' << time_buffer << "] ";
                            std::cout << "Received command: " << command << "; ";
                            std::cout << "Channel: " << channel << "; ";
                            std::cout << std::endl;
                        }

                        histogram_t *const histo_ToF = global_status.histos_ToF[channel];

                        if (histo_ToF != NULL)
                        {
                            histogram_reset(histo_ToF);
                        }

                        histogram_t *const histo_E = global_status.histos_E[channel];

                        if (histo_E != NULL)
                        {
                            histogram_reset(histo_E);
                        }

                        histogram2D_t *const histo_EToF = global_status.histos_EToF[channel];

                        if (histo_EToF != NULL)
                        {
                            histogram2D_reset(histo_EToF);
                        }

                        global_status.partial_counts[channel] = 0;

                        actions::generic::publish_message(global_status, defaults_tofcalc_events_topic, json_event_message);

                        json_decref(json_event_message);
                    }
                }
                else if (command == std::string("reconfigure") && json_arguments != NULL)
                {
                    std::cout << "Reconfiguration" << std::endl;

                    json_t *json_channels = json_object_get(json_arguments, "channels");

                    if (json_channels != NULL && json_is_array(json_channels))
                    {
                        size_t index;
                        json_t *value;

                        global_status.reference_channels.clear();

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

                                json_t *json_reference = json_object_get(value, "reference");

                                // We create a ToF histogram only if the channel is not a reference channel
                                // because we use reference channels as start pulses and we do not want to
                                // merge the ToF of all the other channels in one histogram.
                                if (json_is_true(json_reference))
                                {
                                    global_status.reference_channels.insert(id);

                                    if (global_status.verbosity > 0)
                                    {
                                        char time_buffer[BUFFER_SIZE];
                                        time_string(time_buffer, BUFFER_SIZE, NULL);
                                        std::cout << '[' << time_buffer << "] ";
                                        std::cout << "Channel: " << id << " detected as reference; ";
                                        std::cout << std::endl;
                                    }
                                }
                                else
                                {
                                    auto iter = std::find(global_status.reference_channels.begin(),
                                                          global_status.reference_channels.end(),
                                                          id);

                                    // If it was a reference channel we need to erase it
                                    if (iter != global_status.reference_channels.end())
                                    {
                                        global_status.reference_channels.erase(iter);
                                    }

                                    global_status.partial_counts[id] = 0;

                                    unsigned int bins_ToF = defaults_tofcalc_bins_ToF;
                                    double min_ToF = defaults_tofcalc_min_ToF;
                                    double max_ToF = defaults_tofcalc_max_ToF;

                                    json_t *json_bins_ToF = json_object_get(value, "bins_ToF");
                                    if (json_bins_ToF != NULL && json_is_integer(json_bins_ToF)) {
                                        bins_ToF = json_integer_value(json_bins_ToF);
                                    }
                                    json_t *json_min_ToF = json_object_get(value, "min_ToF");
                                    if (json_min_ToF != NULL && json_is_number(json_min_ToF)) {
                                        min_ToF = json_number_value(json_min_ToF);
                                    }
                                    json_t *json_max_ToF = json_object_get(value, "max_ToF");
                                    if (json_max_ToF != NULL && json_is_number(json_max_ToF)) {
                                        max_ToF = json_number_value(json_max_ToF);
                                    }
                
                                    unsigned int bins_E = defaults_tofcalc_bins_E;
                                    double min_E = defaults_tofcalc_min_E;
                                    double max_E = defaults_tofcalc_max_E;

                                    json_t *json_bins_E = json_object_get(value, "bins_E");
                                    if (json_bins_E != NULL && json_is_integer(json_bins_E)) {
                                        bins_E = json_integer_value(json_bins_E);
                                    }
                                    json_t *json_min_E = json_object_get(value, "min_E");
                                    if (json_min_E != NULL && json_is_number(json_min_E)) {
                                        min_E = json_number_value(json_min_E);
                                    }
                                    json_t *json_max_E = json_object_get(value, "max_E");
                                    if (json_max_E != NULL && json_is_number(json_max_E)) {
                                        max_E = json_number_value(json_max_E);
                                    }
                
                                    histogram_t *histo_ToF = global_status.histos_ToF[id];
                                    if (histo_ToF)
                                    {
                                        histogram_destroy(histo_ToF);
                                    }
                                    histogram_t *histo_E = global_status.histos_E[id];
                                    if (histo_E)
                                    {
                                        histogram_destroy(histo_E);
                                    }
                                    histogram2D_t *histo_EToF = global_status.histos_EToF[id];
                                    if (histo_EToF)
                                    {
                                        histogram2D_destroy(histo_EToF);
                                    }

                                    global_status.histos_ToF[id] = histogram_create(bins_ToF,
                                                                                    min_ToF,
                                                                                    max_ToF,
                                                                                    global_status.verbosity);
                                    global_status.histos_E[id] = histogram_create(bins_E,
                                                                                  min_E,
                                                                                  max_E,
                                                                                  global_status.verbosity);
                                    global_status.histos_EToF[id] = histogram2D_create(bins_ToF,
                                                                                       min_ToF,
                                                                                       max_ToF,
                                                                                       bins_E,
                                                                                       min_E,
                                                                                       max_E,
                                                                                       global_status.verbosity);

                                    const std::string event_message = "Reconfiguration of histogram for channel: " + std::to_string(id);

                                    json_t *json_event_message = json_object();
                                    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
                                    json_object_set_new_nocheck(json_event_message, "event", json_string(event_message.c_str()));

                                    actions::generic::publish_message(global_status, defaults_tofcalc_events_topic, json_event_message);

                                    json_decref(json_event_message);
                                }
                            }
                        }
                    }
                }
                else if (command == std::string("quit"))
                {
                    return states::CLOSE_SOCKETS;
                }
            }
        }
    }

    return states::READ_SOCKET;
}

state actions::read_socket(status &global_status)
{
    actions::generic::read_socket(global_status);

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    if (now - global_status.last_publication > std::chrono::seconds(global_status.publish_timeout))
    {
        return states::PUBLISH_DATA;
    }

    return states::READ_SOCKET;
}

state actions::publish_data(status &global_status)
{
    actions::generic::publish_data(global_status);

    return states::PUBLISH_STATUS;
}

state actions::close_sockets(status &global_status)
{
    void *status_socket = global_status.status_socket;
    void *data_socket = global_status.data_socket;
    void *commands_socket = global_status.commands_socket;
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

    const int c = zmq_close(commands_socket);
    if (c != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on commands socket close: ";
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

    actions::generic::publish_message(global_status, defaults_tofcalc_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::CLOSE_SOCKETS;
}

state actions::stop(status&)
{
    return states::STOP;
}
