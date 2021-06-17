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
#include <jansson.h>
}

#include "typedefs.hpp"
#include "states.hpp"
#include "events.hpp"
#include "actions.hpp"

#include "binary_fifo.hpp"

extern "C" {
#include "utilities_functions.h"
#include "socket_functions.h"
#include "jansson_socket_functions.h"
#include "base64.h"
}

#define BUFFER_SIZE 32

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

    json_object_set_new(status_message, "module", json_string("fifo"));
    json_object_set_new(status_message, "timestamp", json_string(time_buffer));
    json_object_set_new(status_message, "msg_ID", json_integer(status_msg_ID));

    send_json_message(status_socket, const_cast<char*>(topic.c_str()), status_message, 1);

    global_status.status_msg_ID += 1;
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

    // Creates the reply socket
    void *reply_socket = zmq_socket(context, ZMQ_REP);
    if (!reply_socket)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on reply socket creation: ";
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
    global_status.reply_socket = reply_socket;
    global_status.abcd_data_socket = abcd_data_socket;

    return states::BIND_SOCKETS;
}

state actions::bind_sockets(status &global_status)
{
    std::string status_address = global_status.status_address;
    std::string reply_address = global_status.reply_address;
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
    const int d = zmq_bind(global_status.reply_socket, reply_address.c_str());
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
    const std::string subscription_topic(global_status.subscription_topic);

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Subscribing to topic: " << subscription_topic << "; ";
        std::cout << std::endl;
    }

    zmq_setsockopt(global_status.abcd_data_socket, ZMQ_SUBSCRIBE, subscription_topic.c_str(), subscription_topic.size());

    return states::PUBLISH_STATUS;
}

state actions::publish_status(status &global_status)
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
        return states::RECEIVE_COMMANDS;
    }

    json_object_set_new_nocheck(status_message,
                                "counts",
                                json_integer(global_status.fifo.count()));
    json_object_set_new_nocheck(status_message,
                                "size",
                                json_integer(global_status.fifo.size()));
    json_object_set_new_nocheck(status_message,
                                "expiration_time",
                                json_integer(global_status.fifo.get_expiration_time() / 1000000000l));

    actions::generic::publish_message(global_status, defaults_fifo_status_topic, status_message);

    json_decref(status_message);

    // Cleaning up the FIFO periodically
    global_status.fifo.update();

    return states::RECEIVE_COMMANDS;
}

state actions::receive_commands(status &global_status)
{
    void *reply_socket = global_status.reply_socket;

    json_t *json_message = NULL;

    const int result = receive_json_message(reply_socket, nullptr, &json_message, false, global_status.verbosity);

    if (!json_message || result == EXIT_FAILURE)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Error on receiving JSON commands message";
        std::cout << std::endl;
    }
    else if (json_object_size(json_message) > 0 && result == EXIT_SUCCESS)
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
        json_t *json_request_ID = json_object_get(json_message, "msg_ID");

        const size_t request_ID = json_integer_value(json_request_ID);

        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Request ID: " << request_ID << "; ";
            std::cout << std::endl;
        }

        bool error_flag = false;
        std::string error_string;
        json_t *reply_message = json_object();

        // Warning: we are reusing a json_t object from another one we cannot steal the reference
        //json_result = json_object_set_nocheck(reply_message, "request_ID", json_request_ID);
        //std::cout << "request_ID\tjson_result: " << json_result << "; " << std::endl;
        auto json_result = json_object_set_new_nocheck(reply_message, "request_ID", json_integer(request_ID));

        if (json_command != NULL && json_is_string(json_command))
        {
            const std::string command = json_string_value(json_command);

            if (command == std::string("get_data") && json_arguments != NULL)
            {
                if (global_status.verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Received command: " << command << "; ";
                    std::cout << std::endl;
                }

                json_t *json_from = json_object_get(json_arguments, "from");
                json_t *json_to = json_object_get(json_arguments, "to");

                const std::string from_string = json_string_value(json_from);
                const std::string to_string = json_string_value(json_to);

                if (global_status.verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "From: " << from_string << "; ";
                    std::cout << std::endl;
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "To: " << to_string << "; ";
                    std::cout << std::endl;
                }

                int year = 0;
                int Month = 0;
                int day = 0;
                int hour = 0;
                int minute = 0;
                float second = 0;
                int tzh = 0;
                int tzm = 0;
                std::tm time_struct;

                int sscanf_result = 0;

                // Parsing 'from' string...
                // I HATE C++ why so many lines for such a standard thing?!
                sscanf_result = sscanf(from_string.c_str(),
                                       "%d-%d-%dT%d:%d:%f%d:%dZ",
                                       &year, &Month, &day, &hour, &minute, &second, &tzh, &tzm);
                if (sscanf_result < 6)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Parsing error of 'from' time string";
                    std::cout << std::endl;

                    error_flag = true;
                    error_string += "ERROR: Parsing error of 'from' time string;";
                }

                if (tzh < 0)
                {
                    // Fix the sign on minutes.
                    tzm = -tzm;
                }

                if (global_status.verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "From: ";
                    std::cout << year << "-" << Month << "-" << day << "T" << hour << ":" << minute << ":" << second << "+" << tzh << ":" << tzm << "; ";
                    std::cout << std::endl;
                }

                // Year since 1900
                time_struct.tm_year = year - 1900;
                // Month: 0-11
                time_struct.tm_mon = Month - 1;
                // Day: 1-31
                time_struct.tm_mday = day;
                // Hour: 0-23
                time_struct.tm_hour = hour;
                // Minute: 0-59
                time_struct.tm_min = minute;
                // Second: 0-61 (0-60 in C++11)
                time_struct.tm_sec = (int)second;
                // Daylight saving time information not available
                time_struct.tm_isdst = -1;

                const std::time_t from_time_struct = std::mktime(&time_struct);
                const binary_fifo::time_point from_time = std::chrono::system_clock::from_time_t(from_time_struct);

                // Parsing 'to' string...
                sscanf_result = sscanf(to_string.c_str(),
                                       "%d-%d-%dT%d:%d:%f%d:%dZ",
                                       &year, &Month, &day, &hour, &minute, &second, &tzh, &tzm);
                if (sscanf_result < 6)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Parsing error of 'to' time string";
                    std::cout << std::endl;

                    error_flag = true;
                    error_string += "ERROR: Parsing error of 'from' time string;";
                }

                if (tzh < 0)
                {
                    // Fix the sign on minutes.
                    tzm = -tzm;
                }

                if (global_status.verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "To: ";
                    std::cout << year << "-" << Month << "-" << day << "T" << hour << ":" << minute << ":" << second << "+" << tzh << ":" << tzm << "; ";
                    std::cout << std::endl;
                }

                // Year since 1900
                time_struct.tm_year = year - 1900;
                // Month: 0-11
                time_struct.tm_mon = Month - 1;
                // Day: 1-31
                time_struct.tm_mday = day;
                // Hour: 0-23
                time_struct.tm_hour = hour;
                // Minute: 0-59
                time_struct.tm_min = minute;
                // Second: 0-61 (0-60 in C++11)
                time_struct.tm_sec = (int)second;
                // Daylight saving time information not available
                time_struct.tm_isdst = -1;

                const std::time_t to_time_struct = std::mktime(&time_struct);

                if (global_status.verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";

                    time_string(time_buffer, BUFFER_SIZE, &time_struct);
                    std::cout << "To: " << time_buffer << "; ";
                    std::cout << std::endl;
                }

                const binary_fifo::time_point to_time = std::chrono::system_clock::from_time_t(to_time_struct);

                if (global_status.verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Cleaning up the FIFO before requesting the data; ";
                    std::cout << std::endl;
                }

                global_status.fifo.update();

                if (global_status.verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Retrieving data; ";
                    std::cout << std::endl;
                }

                std::vector<binary_fifo::binary_data> data = global_status.fifo.get_data(from_time, to_time);

                if (global_status.verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Encoding base64 data; ";
                    std::cout << std::endl;
                }

                json_t *json_data = json_array();

                size_t total_size = 0;

                for (auto &this_data: data)
                {
                    total_size += this_data.size();

                    size_t output_size = 0;
                    unsigned char *base64_data = base64_encode(this_data.data(), this_data.size(), &output_size);

                    if (!base64_data)
                    {
                        error_flag = true;
                        error_string += "ERROR: Unable to encode binary data;";
                    }
                    else
                    {
                        json_array_append_new(json_data, json_string(reinterpret_cast<char *>(base64_data)));

                        free(base64_data);
                    }
                }

                if (global_status.verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Data packets: " << data.size() << "; ";
                    std::cout << "Data size: " << total_size << "; ";
                    std::cout << std::endl;
                }

                json_result = json_object_set_new_nocheck(reply_message,
                                                          "data",
                                                          json_data);
                std::cout << "data\tjson_result: " << json_result << "; " << std::endl;

                json_result = json_object_set_new_nocheck(reply_message,
                                                          "size",
                                                          json_integer(total_size));
                std::cout << "size\tjson_result: " << json_result << "; " << std::endl;
            }
            else
            {
                error_flag = true;
                error_string += "ERROR: Parsing error of 'from' time string;";
            }
        }

        if (error_flag)
        {
            json_result = json_object_set_new_nocheck(reply_message, "type", json_string("error"));
            std::cout << "type\tjson_result: " << json_result << "; " << std::endl;
            json_result = json_object_set_new_nocheck(reply_message, "error", json_string(error_string.c_str()));
            std::cout << "error\tjson_result: " << json_result << "; " << std::endl;
        }
        else
        {
            json_result = json_object_set_new_nocheck(reply_message, "type", json_string("data"));
            std::cout << "type\tjson_result: " << json_result << "; " << std::endl;
        }

        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);

        json_object_set_new_nocheck(reply_message, "module", json_string("fifo"));
        json_object_set_new_nocheck(reply_message, "timestamp", json_string(time_buffer));
        json_object_set_new_nocheck(reply_message, "msg_ID", json_integer(global_status.reply_msg_ID));

        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << "[" << time_buffer << "] ";
            std::cout << "Encoding JSON object; ";
            std::cout << std::endl;
        }

        char *output_buffer = json_dumps(reply_message, JSON_COMPACT);

        if (output_buffer) {
            if (global_status.verbosity > 2)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Reply buffer: ";
                std::cout << output_buffer;
                std::cout << std::endl;
            }

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Sending message of size: " << strlen(output_buffer) << "; ";
                std::cout << std::endl;
            }

            send_byte_message(reply_socket, nullptr, output_buffer, strlen(output_buffer), 1);

            free(output_buffer);
        }
        else
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR: Unable to create JSON message; ";
            std::cout << std::endl;

            const std::string message = "{\"type\": \"error\", \"error\": \"ERROR: Unable to create JSON message\"}";
            send_byte_message(reply_socket,
                              nullptr,
                              (void *)message.c_str(),
                              message.size(),
                              1);
        }

        global_status.reply_msg_ID += 1;

        json_decref(reply_message);
    }

    json_decref(json_message);

    return states::READ_SOCKET;
}

state actions::read_socket(status &global_status)
{
    void *abcd_data_socket = global_status.abcd_data_socket;

    char *topic;
    uint8_t *input_buffer;
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

        if (topic_string.find(global_status.subscription_topic) == 0)
        {
            const clock_t event_start = clock();

            const size_t data_size = size;

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Data size: " << data_size << "; ";
                std::cout << std::endl;
            }

            binary_fifo::binary_data new_data;
            new_data.resize(data_size);

            memcpy(new_data.data(), input_buffer, data_size * sizeof(uint8_t));

            global_status.fifo.push(new_data);

            const clock_t event_stop = clock();

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Elaboration time: " << (float)(event_stop - event_start) / CLOCKS_PER_SEC * 1000 << " ms; ";
                std::cout << std::endl;
            }

        }

        // Remember to free buffers
        free(topic);
        free(input_buffer);

        result = receive_byte_message(abcd_data_socket, &topic, (void **)(&input_buffer), &size, true, global_status.verbosity);
    }

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    if (now - global_status.last_publication > std::chrono::seconds(defaults_fifo_publish_timeout))
    {
        return states::PUBLISH_STATUS;
    }

    return states::READ_SOCKET;
}

state actions::close_sockets(status &global_status)
{
    void *status_socket = global_status.status_socket;
    void *reply_socket = global_status.reply_socket;
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

    const int c = zmq_close(reply_socket);
    if (c != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on reply socket close: ";
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

    actions::generic::publish_message(global_status, defaults_fifo_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::CLOSE_SOCKETS;
}

state actions::stop(status&)
{
    return states::STOP;
}
