// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

// For calloc()
//#include <cstdlib>
#include <ctime>
// For memcpy()
#include <cstring>
#include <string>
#include <iostream>
#include <string>
#include <vector>
// For unique_ptr
#include <memory>

#include <zmq.h>
#include <json/json.h>

#include "utilities_functions.hpp"
#include "socket_functions.hpp"

//! Sends a binary message
/*! \param socket the ØMQ socket.
    \param topic a string describing the topic of the message for PUB sockets, can be empty otherwise.
    \param buffer a pointer to the binary buffer to be sent.
    \param size the binary buffer size.
    \param verbosity if more than zero, activates some debug output (default: 0).
    \return true if successfull false otherwise.
 */
bool socket_functions::send_byte_message(void *socket, \
                                         std::string topic, \
                                         void *buffer, \
                                         size_t size, \
                                         unsigned int verbosity)
{
    if (topic.length() > 0)
    {
        topic += ' ';
    }

    const size_t topic_size = topic.length();
    const size_t envelope_size = topic_size + size;

    // Creates an empty ØMQ message
    zmq_msg_t envelope;

    // Allocates the necessary memory for the message
    const int envelope_init_result = zmq_msg_init_size(&envelope, envelope_size);
    if (envelope_init_result != 0)
    {
        std::cerr << '[' << utilities_functions::time_string();
        std::cerr << "] ZeroMQ Error on send, on envelope init: ";
        std::cerr << zmq_strerror(errno) << std::endl;

        zmq_msg_close(&envelope);

        return false;
    }
    
    // Stores the data into the message
    memcpy(zmq_msg_data(&envelope), topic.c_str(), topic_size);
    // Some pointer arithmetics with char pointers, as it is illegal for void pointers
    memcpy((void*)((char*)zmq_msg_data(&envelope) + topic_size), buffer, size);

    if (verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string();
        std::cout << "] Message size: " << envelope_size << std::endl;
    }

    // Sends the message
    const int envelope_send_result = zmq_msg_send(&envelope, socket, 0);
    if (envelope_send_result != static_cast<int>(envelope_size))
    {
        std::cerr << '[' << utilities_functions::time_string();
        std::cerr << "] ZeroMQ Error on send, on envelope send: ";
        std::cerr << zmq_strerror(errno) << std::endl;

        zmq_msg_close(&envelope);

        return false;
    }

    // Releases message
    zmq_msg_close(&envelope);

    return true;
}

//! Receives a binary message
/*! \param socket the ØMQ socket.
    \param verbosity if more than zero, activates some debug output (default: 0).
    \return a pointer to the received binary object, if an error occured or the function timedout the pointer is null. The buffer shall be freed with free().
 */
std::vector<char> socket_functions::receive_byte_message(void *socket, unsigned int verbosity)
{
    // Creates an empty ØMQ message
    zmq_msg_t envelope;

    // Initializes the memory for the message
    const int envelope_init_result = zmq_msg_init(&envelope);
    if (envelope_init_result != 0)
    {
        std::cerr << '[' << utilities_functions::time_string();
        std::cerr << "] ZeroMQ Error on receive, on envelope init: ";
        std::cerr << zmq_strerror(errno) << std::endl;

        return std::vector<char>();
    }

    // Receives message in non-bloking mode. In case of failure it shall return -1.
    // If there are no messages available, the function shall fail with errno set to EAGAIN.
    const int envelope_receive_result = zmq_msg_recv(&envelope, socket, ZMQ_DONTWAIT);
    const int error_number = errno;

    if (envelope_receive_result < 0 && error_number != EAGAIN)
    {
        std::cerr << '[' << utilities_functions::time_string();
        std::cerr << "] ZeroMQ Error on receive, on envelope receive: ";
        std::cerr << zmq_strerror(errno);
        std::cerr << " (errno: " << error_number << ')' << std::endl;

        // Release message
        zmq_msg_close (&envelope);

        return std::vector<char>();
    }
    else if (envelope_receive_result < 0 && error_number == EAGAIN)
    {
        // No message available, so release message
        zmq_msg_close (&envelope);

        return std::vector<char>();
    }
    else
    {
        const size_t message_size = zmq_msg_size(&envelope);

        // Allocates the necessary buffer
        std::vector<char> buffer(message_size);

        // Copies the message string
        memcpy(buffer.data(), zmq_msg_data(&envelope), message_size);

        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string();
            std::cout << "] Message length: '" << message_size;
            std::cout << std::endl;
        }
        
        // Release message
        zmq_msg_close (&envelope);

        return buffer;
    }
}

//! Sends a JSON message
/*! \param socket the ØMQ socket.
    \param topic a string describing the topic of the message for PUB sockets, can be empty otherwise.
    \param json_message the Json::Value object to be sent.
    \param verbosity if more than zero, activates some debug output (default: 0).
    \return true if successfull false otherwise.
 */
bool socket_functions::send_json_message(void *socket, \
                                         std::string topic, \
                                         Json::Value json_message, \
                                         unsigned int verbosity)
{
    //Json::FastWriter writer;
    // Serializes the JSON object
    //std::string message = writer.write(json_message);
    Json::StreamWriterBuilder builder;
    std::string message = Json::writeString(builder, json_message);


    std::string envelope_string;

    if (topic.length() > 0)
    {
        envelope_string += topic;
        envelope_string += ' ';
    }

    envelope_string += message;

    if (verbosity > 0)
    {
        std::cout << '[' << utilities_functions::time_string() << "] ";
        std::cout << "Message: '" << envelope_string << "' ";
        std::cout << "(length: " << envelope_string.size() << ")";
        std::cout << std::endl;
    }

    // Creates an empty ØMQ message
    zmq_msg_t envelope;

    // Allocates the necessary memory for the message
    const int envelope_init_result = zmq_msg_init_size(&envelope, envelope_string.length());
    if (envelope_init_result != 0)
    {
        std::cerr << '[' << utilities_functions::time_string();
        std::cerr << "] ZeroMQ Error on send, on envelope init: ";
        std::cerr << zmq_strerror(errno) << std::endl;

        zmq_msg_close(&envelope);

        return false;
    }
    
    // Stores the data into the message
    memcpy(zmq_msg_data(&envelope), envelope_string.c_str(), envelope_string.length());

    // Sends the message
    const int envelope_send_result = zmq_msg_send(&envelope, socket, 0);
    if (envelope_send_result != static_cast<int>(envelope_string.length()))
    {
        std::cerr << '[' << utilities_functions::time_string();
        std::cerr << "] ZeroMQ Error on send, on envelope send: ";
        std::cerr << zmq_strerror(errno) << std::endl;

        zmq_msg_close(&envelope);

        return false;
    }

    // Releases message
    zmq_msg_close(&envelope);

    return true;
}

//! Receives a JSON message
/*! \param socket the ØMQ socket.
    \param verbosity if more than zero, activates some debug output (default: 0).
    \return the received JSON object, if an error occured or the function timedout the object is empty
 */
Json::Value socket_functions::receive_json_message(void *socket, \
                                                   unsigned int verbosity)
{
    // Creates an empty ØMQ message
    zmq_msg_t envelope;

    // Initializes the memory for the message
    const int envelope_init_result = zmq_msg_init(&envelope);
    if (envelope_init_result != 0)
    {
        std::cerr << '[' << utilities_functions::time_string();
        std::cerr << "] ZeroMQ Error on receive, on envelope init: ";
        std::cerr << zmq_strerror(errno) << std::endl;

        return Json::Value();
    }

    // Receives message in non-bloking mode. In case of failure it shall return -1.
    // If there are no messages available, the function shall fail with errno set to EAGAIN.
    const int envelope_receive_result = zmq_msg_recv(&envelope, socket, ZMQ_DONTWAIT);
    const int error_number = errno;

    if (envelope_receive_result < 0 && error_number != EAGAIN)
    {
        std::cerr << '[' << utilities_functions::time_string();
        std::cerr << "] ZeroMQ Error on receive, on envelope receive: ";
        std::cerr << zmq_strerror(errno);
        std::cerr << " (errno: " << error_number << ')' << std::endl;

        // Release message
        zmq_msg_close (&envelope);

        return Json::Value();
    }
    else if (envelope_receive_result < 0 && error_number == EAGAIN)
    {
        // No message available, so release message
        zmq_msg_close (&envelope);

        return Json::Value();
    }
    else
    {
        // Reads the message string
        char *data_buffer = reinterpret_cast<char*>(zmq_msg_data(&envelope));

        // Reads the message string
        std::string message(data_buffer);

        // Resizes the string to the correct size
        message.resize(zmq_msg_size(&envelope));
        
        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string();
            std::cout << "] Message: '" << message;
            std::cout << "' length: " << message.length();
            std::cout << ", " << envelope_receive_result;
            std::cout << ", " << zmq_msg_size(&envelope) << std::endl;
        }
        
        Json::Value json_message;

        // Parses the string ang generates the JSON object
        // This has been deprecated, apparently it was too easy
        //Json::Reader reader;
        //reader.parse(message, json_message);
        // Ahh much better now... Much cleaner and easier
        Json::CharReaderBuilder json_reader_builder;
        std::string json_parsing_error;
        std::unique_ptr<Json::CharReader> const reader(json_reader_builder.newCharReader());

        char const* start = message.c_str();
        char const* stop = start + message.size();
        reader->parse(start, stop, &json_message, &json_parsing_error);

        // Release message
        zmq_msg_close (&envelope);

        // This convoluted way to return the object is to keep compatibility with the Scientific Linux's gcc
        return Json::Value(json_message);
    }
}

//! Receives a JSON message
/*! \param socket the ØMQ socket.
    \param verbosity if more than zero, activates some debug output (default: 0).
    \return the received JSON object, if an error occured or the function timedout the object is empty
 */
Json::Value socket_functions::receive_json_message(void *socket, \
                                                   std::string &topic, \
                                                   unsigned int verbosity)
{
    // Creates an empty ØMQ message
    zmq_msg_t envelope;

    // Initializes the memory for the message
    const int envelope_init_result = zmq_msg_init(&envelope);
    if (envelope_init_result != 0)
    {
        std::cerr << '[' << utilities_functions::time_string();
        std::cerr << "] ZeroMQ Error on receive, on envelope init: ";
        std::cerr << zmq_strerror(errno) << std::endl;

        return Json::Value();
    }

    // Receives message in non-bloking mode. In case of failure it shall return -1.
    // If there are no messages available, the function shall fail with errno set to EAGAIN.
    const int envelope_receive_result = zmq_msg_recv(&envelope, socket, ZMQ_DONTWAIT);
    const int error_number = errno;

    if (envelope_receive_result < 0 && error_number != EAGAIN)
    {
        std::cerr << '[' << utilities_functions::time_string();
        std::cerr << "] ZeroMQ Error on receive, on envelope receive: ";
        std::cerr << zmq_strerror(errno);
        std::cerr << " (errno: " << error_number << ')' << std::endl;

        // Release message
        zmq_msg_close (&envelope);

        return Json::Value();
    }
    else if (envelope_receive_result < 0 && error_number == EAGAIN)
    {
        // No message available, so release message
        zmq_msg_close (&envelope);

        return Json::Value();
    }
    else
    {
        int socket_type = 0;
        size_t option_len = sizeof(socket_type);
        const int getsockopt_result = zmq_getsockopt(socket, ZMQ_TYPE, (void*)(&socket_type), &option_len);
        const int error_number = errno;

        if (getsockopt_result < 0)
        {
            std::cerr << '[' << utilities_functions::time_string();
            std::cerr << "] ZeroMQ Error on getsockopt: ";
            std::cerr << zmq_strerror(errno);
            std::cerr << " (errno: " << error_number << ')' << std::endl;
        }

        // Reads the message string
        char *data_buffer = reinterpret_cast<char*>(zmq_msg_data(&envelope));
        char *json_buffer = nullptr;

        size_t topic_length = 0;

        if (socket_type == ZMQ_SUB)
        {
            // Looking for a topic to isolate
            char *position = strchr(data_buffer, ' ');
            *position = '\0';

            topic = std::string(data_buffer);

            // Restoring the original message
            *position = ' ';

            // Moving the cursor to the right of the space
            json_buffer = position + 1;

            topic_length = topic.size() > 0 ? topic.size() + 1 : 0;
        }
        else
        {
            json_buffer = data_buffer;
        }

        // Reads the message string
        std::string message(json_buffer);

        // Resizes the string to the correct size
        message.resize(zmq_msg_size(&envelope) - topic_length);
        
        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string();
            std::cout << "] Message: '" << message;
            std::cout << "' length: " << message.length();
            std::cout << ", " << envelope_receive_result;
            std::cout << ", " << zmq_msg_size(&envelope) << std::endl;
        }
        
        Json::Value json_message;

        // Parses the string ang generates the JSON object
        // This has been deprecated, apparently it was too easy
        //Json::Reader reader;
        //reader.parse(message, json_message);
        // Ahh much better now... Much cleaner and easier
        Json::CharReaderBuilder json_reader_builder;
        std::string json_parsing_error;
        std::unique_ptr<Json::CharReader> const reader(json_reader_builder.newCharReader());

        char const* start = message.c_str();
        char const* stop = start + message.size();
        reader->parse(start, stop, &json_message, &json_parsing_error);


        // Release message
        zmq_msg_close (&envelope);

        // This convoluted way to return the object is to keep compatibility with the Scientific Linux's gcc
        return Json::Value(json_message);
    }
}

//! Receives a message and dumps it
/*! \param socket the ØMQ socket.
    \return true if a message was received
 */
bool socket_functions::discard_message(void *socket)
{
    // Creates an empty ØMQ message
    zmq_msg_t envelope;

    // Initializes the memory for the message
    const int envelope_init_result = zmq_msg_init(&envelope);
    if (envelope_init_result != 0)
    {
        std::cerr << '[' << utilities_functions::time_string();
        std::cerr << "] ZeroMQ Error on receive, on envelope init: ";
        std::cerr << zmq_strerror(errno) << std::endl;

        return false;
    }

    // Receives message in non-bloking mode. In case of failure it shall return -1.
    // If there are no messages available, the function shall fail with errno set to EAGAIN.
    const int envelope_receive_result = zmq_msg_recv(&envelope, socket, ZMQ_DONTWAIT);
    const int error_number = errno;

    // Release message
    zmq_msg_close (&envelope);

    if (envelope_receive_result < 0 && error_number != EAGAIN)
    {
        std::cerr << '[' << utilities_functions::time_string();
        std::cerr << "] ZeroMQ Error on receive, on envelope receive: ";
        std::cerr << zmq_strerror(errno);
        std::cerr << " (errno: " << error_number << ')' << std::endl;

        return false;
    }
    else if (envelope_receive_result < 0 && error_number == EAGAIN)
    {
        // No message available
        return false;
    }
    else
    {
        return true;
    }
}

