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

void actions::generic::clear_memory(status &global_status)
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

    for (auto &pair: global_status.histos_EvsToF)
    {
        histogram2D_t *const histo_EvsToF = pair.second;

        if (histo_EvsToF != NULL)
        {
            histogram2D_destroy(histo_EvsToF);
        }
    }
    global_status.histos_EvsToF.clear();

    for (auto &pair: global_status.histos_EvsE)
    {
        histogram2D_t *const histo_EvsE = pair.second;

        if (histo_EvsE != NULL)
        {
            histogram2D_destroy(histo_EvsE);
        }
    }
    global_status.histos_EvsE.clear();

    global_status.counts_partial.clear();
    global_status.counts_total.clear();
}

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
    }

    for (const unsigned int &channel: global_status.active_channels)
    {
        const unsigned int channel_total_counts = global_status.counts_total[channel];
        const unsigned int channel_partial_counts = global_status.counts_partial[channel];
        const double channel_rate = channel_partial_counts / pubtime;

        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Publishing status for active channel: " << channel << "; ";
            std::cout << std::endl;
        }

        json_t *channel_status = json_object();

        json_object_set_new_nocheck(channel_status, "id", json_integer(channel));
        json_object_set_new_nocheck(channel_status, "enabled", json_true());
        json_object_set_new_nocheck(channel_status, "rate", json_real(channel_rate));
        json_object_set_new_nocheck(channel_status, "counts", json_integer(channel_total_counts));
        json_array_append_new(channels_statuses, channel_status);

        json_array_append_new(active_channels, json_integer(channel));
    }

    json_object_set_new_nocheck(status_message, "statuses", channels_statuses);
    json_object_set_new_nocheck(status_message, "reference_channels", reference_channels);
    json_object_set_new_nocheck(status_message, "active_channels", active_channels);
    json_object_set_new_nocheck(status_message, "config", json_deep_copy(global_status.config));

    actions::generic::publish_message(global_status, defaults_tofcalc_status_topic, status_message);

    json_decref(status_message);

    // Resetting counts
    for (auto &pair: global_status.counts_partial)
    {
        pair.second = 0;
    }

    // Scaling histograms right after the publication so the counts in between
    // two publications are not decaying
    if (global_status.time_decay_enabled) {
        const double time_decay_constant = exp(-pubtime / global_status.time_decay_tau);

        for (const unsigned int &channel: global_status.active_channels)
        {
            histogram_t *const histo_ToF = global_status.histos_ToF[channel];
            histogram_t *const histo_E = global_status.histos_E[channel];
            histogram2D_t *const histo_EvsToF = global_status.histos_EvsToF[channel];
            histogram2D_t *const histo_EvsE = global_status.histos_EvsE[channel];

            if (histo_ToF && histo_E && histo_EvsToF && histo_EvsE) {
                if (global_status.verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Scaling histograms for active channel: " << channel << "; ";
                    std::cout << std::endl;
                }

                histogram_scale(histo_ToF, time_decay_constant);
                histogram_scale(histo_E, time_decay_constant);
                histogram2D_scale(histo_EvsToF, time_decay_constant);
                histogram2D_scale(histo_EvsE, time_decay_constant);

                histogram_counts_clear_minimum(histo_ToF, global_status.time_decay_minimum);
                histogram_counts_clear_minimum(histo_E, global_status.time_decay_minimum);
                histogram2D_counts_clear_minimum(histo_EvsToF, global_status.time_decay_minimum);
                histogram2D_counts_clear_minimum(histo_EvsE, global_status.time_decay_minimum);
            }
        }
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

    json_t *channels_data = json_array();
    if (channels_data == NULL)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to create channels_data json; ";
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

        json_t *channel_data = json_object();

        json_object_set_new_nocheck(channel_data, "id", json_integer(channel));
        json_object_set_new_nocheck(channel_data, "enabled", json_true());
        json_object_set_new_nocheck(channel_data, "reference", json_true());

        json_array_append_new(channels_data, channel_data);
    }

    for (const unsigned int &channel: global_status.active_channels)
    {
        histogram_t *const histo_ToF = global_status.histos_ToF[channel];
        histogram_t *const histo_E = global_status.histos_E[channel];
        histogram2D_t *const histo_EvsToF = global_status.histos_EvsToF[channel];
        histogram2D_t *const histo_EvsE = global_status.histos_EvsE[channel];
        const unsigned int channel_total_counts = global_status.counts_total[channel];
        const unsigned int channel_partial_counts = global_status.counts_partial[channel];
        const double channel_rate = channel_partial_counts / pubtime;

        if (histo_ToF && histo_E && histo_EvsToF && histo_EvsE) {
            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Publishing data for active channel: " << channel << "; ";
                std::cout << std::endl;
            }

            json_t *histo_ToF_data = histogram_to_json(histo_ToF);
            json_t *histo_E_data = histogram_to_json(histo_E);
            json_t *histo_EvsToF_data = histogram2D_to_json(histo_EvsToF);
            json_t *histo_EvsE_data = histogram2D_to_json(histo_EvsE);

            json_t *channel_data = json_object();

            json_object_set_new_nocheck(channel_data, "id", json_integer(channel));
            json_object_set_new_nocheck(channel_data, "enabled", json_true());
            json_object_set_new_nocheck(channel_data, "reference", json_false());
            json_object_set_new_nocheck(channel_data, "rate", json_real(channel_rate));
            json_object_set_new_nocheck(channel_data, "counts", json_integer(channel_total_counts));
            json_object_set_new_nocheck(channel_data, "ToF", histo_ToF_data);
            json_object_set_new_nocheck(channel_data, "energy", histo_E_data);
            json_object_set_new_nocheck(channel_data, "EvsToF", histo_EvsToF_data);
            json_object_set_new_nocheck(channel_data, "EvsE", histo_EvsE_data);

            json_array_append_new(channels_data, channel_data);

            json_array_append_new(active_channels, json_integer(channel));
        }
    }

    json_object_set_new_nocheck(status_message, "data", channels_data);
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
                                histogram2D_t *const histo_EvsToF = global_status.histos_EvsToF[that_event.channel];
                                histogram2D_t *const histo_EvsE = global_status.histos_EvsE[that_event.channel];

                                // This is not really an error as a user might not be interested on
                                // the ToF of this channel
                                if (!histo_ToF || !histo_E || !histo_EvsToF || !histo_EvsE) {
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
                                    if (histo_EvsToF->min_x <= time_of_flight && time_of_flight < histo_EvsToF->max_x &&
                                        histo_EvsToF->min_y <= that_event.qlong && that_event.qlong < histo_EvsToF->max_y) {
                                        histogram_fill(histo_ToF, time_of_flight);
                                        histogram_fill(histo_E, that_event.qlong);
                                        histogram2D_fill(histo_EvsToF, time_of_flight, that_event.qlong);
                                        histogram2D_fill(histo_EvsE, that_event.qlong, this_event.qlong);
                                        global_status.counts_partial[that_event.channel] += 1;
                                        global_status.counts_total[that_event.channel] += 1;

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
                                histogram2D_t *const histo_EvsToF = global_status.histos_EvsToF[that_event.channel];
                                histogram2D_t *const histo_EvsE = global_status.histos_EvsE[that_event.channel];

                                // This is not really an error as a user might not be interested on
                                // the ToF of this channel
                                if (!histo_ToF || !histo_E || !histo_EvsToF) {
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
                                    if (histo_EvsToF->min_x <= time_of_flight && time_of_flight < histo_EvsToF->max_x &&
                                        histo_EvsToF->min_y <= that_event.qlong && that_event.qlong < histo_EvsToF->max_y) {
                                        histogram_fill(histo_ToF, time_of_flight);
                                        histogram_fill(histo_E, that_event.qlong);
                                        histogram2D_fill(histo_EvsToF, time_of_flight, that_event.qlong);
                                        histogram2D_fill(histo_EvsE, that_event.qlong, this_event.qlong);
                                        global_status.counts_partial[that_event.channel] += 1;
                                        global_status.counts_total[that_event.channel] += 1;

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
    actions::generic::clear_memory(global_status);

    json_t * const config = global_status.config;

    json_t *json_ns_per_sample = json_object_get(config, "ns_per_sample");

    double ns_per_sample = global_status.ns_per_sample;

    if (json_ns_per_sample != NULL && json_is_number(json_ns_per_sample)) {
        ns_per_sample = json_number_value(json_ns_per_sample);

        if (global_status.verbosity > 0) {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Setting the conversion factor to: " << ns_per_sample << " ns/sample; ";
            std::cout << std::endl;
        }
    }
    json_object_set_nocheck(config, "ns_per_sample", json_real(ns_per_sample));

    global_status.ns_per_sample = ns_per_sample;

    json_t *json_time_decay = json_object_get(config, "time_decay");

    bool time_decay_enabled = defaults_tofcalc_time_decay_enabled;
    double time_decay_tau = defaults_tofcalc_time_decay_tau;
    double time_decay_minimum = defaults_tofcalc_time_decay_minimum;

    if (json_time_decay != NULL && json_is_object(json_time_decay))
    {
        time_decay_enabled = json_is_true(json_object_get(json_time_decay, "enable"));

        json_t *json_tau = json_object_get(json_time_decay, "tau");

        if (json_is_number(json_tau) && json_number_value(json_tau) > 0) {
            time_decay_tau = json_number_value(json_tau);
        } else {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "WARNING: Invalid value for time_decay_tau, got: " << json_number_value(json_tau) << "; ";
            std::cout << std::endl;
        }

        json_t *json_minimum = json_object_get(json_time_decay, "counts_minimum");

        if (json_is_number(json_minimum) && json_number_value(json_minimum) >= 0) {
            time_decay_minimum = json_number_value(json_minimum);
        } else {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "WARNING: Invalid value for time_decay_minimum, got: " << json_number_value(json_minimum) << "; ";
            std::cout << std::endl;
        }

        if (global_status.verbosity > 0) {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Time decay enabled: " << time_decay_enabled << "; ";
            std::cout << "Time decay tau: " << time_decay_tau << "; ";
            std::cout << "Time decay minimum: " << time_decay_minimum << "; ";
            std::cout << std::endl;
        }
    } else {
        json_object_set_new_nocheck(config, "time_decay", json_object());
        json_time_decay = json_object_get(config, "time_decay");
    }

    json_object_set_nocheck(json_time_decay, "enable", time_decay_enabled ? json_true() : json_false());
    json_object_set_nocheck(json_time_decay, "tau", json_real(time_decay_tau));
    json_object_set_nocheck(json_time_decay, "counts_minimum", json_real(time_decay_minimum));

    global_status.time_decay_enabled = time_decay_enabled;
    global_status.time_decay_tau = time_decay_tau;
    global_status.time_decay_minimum = time_decay_minimum;

    json_t *json_channels = json_object_get(config, "channels");

    if (json_channels != NULL && json_is_array(json_channels))
    {
        size_t index;
        json_t *value;

        json_array_foreach(json_channels, index, value) {
            // This block of code that uses the index and value variables
            // that are set in the json_array_foreach() macro.

            // The id may be a single integer or an array of integers
            json_t *json_id = json_object_get(value, "id");

            std::vector<int> channel_ids;

            if (json_id != NULL && json_is_integer(json_id)) {
                const int id = json_number_value(json_id);
                channel_ids.push_back(id);
            } else if (json_id != NULL && json_is_array(json_id)) {
                size_t id_index;
                json_t *id_value;

                json_array_foreach(json_id, id_index, id_value) {
                    if (id_value != NULL && json_is_integer(id_value)) {
                        const int id = json_number_value(id_value);
                        channel_ids.push_back(id);
                    }
                }
            }

            if (channel_ids.size() > 0) {
                if (global_status.verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Found channel(s): ";
                    for (auto& id : channel_ids) {
                        std::cout << id << " ";
                    }
                    std::cout << "; ";
                    std::cout << std::endl;
                }

                const bool is_reference = json_is_true(json_object_get(value, "reference"));

                // We create a ToF histogram only if the channel is not a reference channel
                // because we use reference channels as start pulses and we do not want to
                // merge the ToF of all the other channels in one histogram.
                if (is_reference)
                {
                    json_object_set_nocheck(value, "reference", json_true());

                    for (const auto& id : channel_ids) {
                        global_status.reference_channels.insert(id);

                        if (global_status.verbosity > 0)
                        {
                            char time_buffer[BUFFER_SIZE];
                            time_string(time_buffer, BUFFER_SIZE, NULL);
                            std::cout << '[' << time_buffer << "] ";
                            std::cout << "Channel: " << id << " detected as reference; ";
                            std::cout << std::endl;
                        }

                        const std::string event_message = "Configuration of channel " + std::to_string(id) + " as reference";

                        json_t *json_event_message = json_object();
                        json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
                        json_object_set_new_nocheck(json_event_message, "event", json_string(event_message.c_str()));

                        actions::generic::publish_message(global_status, defaults_tofcalc_events_topic, json_event_message);

                        json_decref(json_event_message);
                    }
                }
                else
                {
                    json_object_set_nocheck(value, "reference", json_false());

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

                    // Rewriting the values to be sure that the configuration
                    // has the proper format.
                    json_object_set_nocheck(value, "bins_ToF", json_integer(bins_ToF));
                    json_object_set_nocheck(value, "min_ToF", json_real(min_ToF));
                    json_object_set_nocheck(value, "max_ToF", json_real(max_ToF));

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

                    json_object_set_nocheck(value, "bins_E", json_integer(bins_E));
                    json_object_set_nocheck(value, "min_E", json_real(min_E));
                    json_object_set_nocheck(value, "max_E", json_real(max_E));

                    for (const auto& id : channel_ids) {
                        global_status.active_channels.insert(id);

                        if (global_status.verbosity > 0)
                        {
                            char time_buffer[BUFFER_SIZE];
                            time_string(time_buffer, BUFFER_SIZE, NULL);
                            std::cout << '[' << time_buffer << "] ";
                            std::cout << "Channel: " << id << " detected as active; ";
                            std::cout << std::endl;
                        }

                        global_status.histos_ToF[id] = histogram_create(bins_ToF,
                                                                        min_ToF,
                                                                        max_ToF,
                                                                        global_status.verbosity);

                        global_status.histos_E[id] = histogram_create(bins_E,
                                                                      min_E,
                                                                      max_E,
                                                                      global_status.verbosity);

                        global_status.histos_EvsToF[id] = histogram2D_create(bins_ToF,
                                                                             min_ToF,
                                                                             max_ToF,
                                                                             bins_E,
                                                                             min_E,
                                                                             max_E,
                                                                             global_status.verbosity);

                        global_status.histos_EvsE[id] = histogram2D_create(bins_E,
                                                                           min_E,
                                                                           max_E,
                                                                           bins_E,
                                                                           min_E,
                                                                           max_E,
                                                                           global_status.verbosity);

                        global_status.counts_partial[id] = 0;
                        global_status.counts_total[id] = 0;

                        const std::string event_message = "Configuration of histograms for channel: " + std::to_string(id);

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

    if (global_status.verbosity > 1)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Receiving command... ";
        std::cout << std::endl;
    }

    json_t *json_message = NULL;

    const int result = receive_json_message(commands_socket, NULL, &json_message, false, global_status.verbosity);

    if (!json_message || result == EXIT_FAILURE)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Error on receiving JSON commands message";
        std::cout << std::endl;
    }
    else
    {
        if (global_status.verbosity > 1)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Received command; ";
            std::cout << std::endl;
        }

        json_t *json_command = json_object_get(json_message, "command");
        json_t *json_arguments = json_object_get(json_message, "arguments");

        if (json_command != NULL)
        {
            const std::string command = json_string_value(json_command);

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Received command: " << command << "; ";
                std::cout << std::endl;
            }

            if (command == std::string("reset") && json_arguments != NULL)
            {
                json_t *json_channel = json_object_get(json_arguments, "channel");

                if (json_channel != NULL && json_is_string(json_channel))
                {
                    const std::string channel_string = json_string_value(json_channel);

                    if (global_status.verbosity > 0)
                    {
                        char time_buffer[BUFFER_SIZE];
                        time_string(time_buffer, BUFFER_SIZE, NULL);
                        std::cout << '[' << time_buffer << "] ";
                        std::cout << "Received command: " << command << "; ";
                        std::cout << "Channel: " << channel_string << "; ";
                        std::cout << std::endl;
                    }

                    if (channel_string == std::string("all"))
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

                        for (auto &pair: global_status.histos_EvsToF)
                        {
                            histogram2D_t *const histo_EvsToF = pair.second;

                            if (histo_EvsToF != NULL)
                            {
                                histogram2D_reset(histo_EvsToF);
                            }
                        }

                        for (auto &pair: global_status.histos_EvsE)
                        {
                            histogram2D_t *const histo_EvsE = pair.second;

                            if (histo_EvsE != NULL)
                            {
                                histogram2D_reset(histo_EvsE);
                            }
                        }

                        for (auto &pair: global_status.counts_partial)
                        {
                            pair.second = 0;
                        }
                        for (auto &pair: global_status.counts_total)
                        {
                            pair.second = 0;
                        }

                        json_t *json_event_message = json_object();
                        json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
                        json_object_set_new_nocheck(json_event_message, "event", json_string("Reset of all channels"));

                        actions::generic::publish_message(global_status, defaults_tofcalc_events_topic, json_event_message);

                        json_decref(json_event_message);
                    }
                }
                else if (json_channel != NULL && json_is_integer(json_channel))
                {
                    const int channel = json_integer_value(json_channel);

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

                    histogram2D_t *const histo_EvsToF = global_status.histos_EvsToF[channel];

                    if (histo_EvsToF != NULL)
                    {
                        histogram2D_reset(histo_EvsToF);
                    }

                    histogram2D_t *const histo_EvsE = global_status.histos_EvsE[channel];

                    if (histo_EvsE != NULL)
                    {
                        histogram2D_reset(histo_EvsE);
                    }

                    global_status.counts_partial[channel] = 0;
                    global_status.counts_total[channel] = 0;

                    std::string event_message = "Reset of channel: " + std::to_string(channel);

                    json_t *json_event_message = json_object();
                    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
                    json_object_set_new_nocheck(json_event_message, "event", json_string(event_message.c_str()));

                    actions::generic::publish_message(global_status, defaults_tofcalc_events_topic, json_event_message);

                    json_decref(json_event_message);
                }
            }
            else if (command == std::string("reconfigure") && json_arguments != NULL)
            {
                if (global_status.verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Reconfiguration; ";
                    std::cout << std::endl;
                }

                json_t *json_config = json_object_get(json_arguments, "config");

                if (json_config != NULL && json_is_object(json_config))
                {
                    global_status.config = json_deep_copy(json_config);

                    const std::string event_message = "Reconfiguration";

                    json_t *json_event_message = json_object();
                    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
                    json_object_set_new_nocheck(json_event_message, "event", json_string(event_message.c_str()));

                    actions::generic::publish_message(global_status, defaults_tofcalc_events_topic, json_event_message);

                    json_decref(json_event_message);

                    json_decref(json_message);

                    return states::APPLY_CONFIG;
                }
            }
            else if (command == std::string("quit"))
            {
                return states::CLOSE_SOCKETS;
            }
        }
    }

    json_decref(json_message);

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
