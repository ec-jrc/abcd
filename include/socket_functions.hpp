#ifndef __SOCKET_FUNCTIONS_HPP__
#define __SOCKET_FUNCTIONS_HPP__ 1

#include <string>
#include <vector>

#include <zmq.h>
#include <json/json.h>

#include "utilities_functions.hpp"

namespace socket_functions {
    //! Sends a binary message
    /*! \param socket the ØMQ socket.
        \param topic a string describing the topic of the message for PUB sockets, can be empty otherwise.
        \param buffer a pointer to the binary buffer to be sent.
        \param size the binary buffer size.
        \param verbosity if more than zero, activates some debug output (default: 0).
        \return true if successfull false otherwise.
    */
    bool send_byte_message(void *socket, \
                           std::string topic, \
                           void *buffer, \
                           size_t size, \
                           unsigned int verbosity = 0);

    //! Receives a binary message
    /*! \param socket the ØMQ socket.
        \param verbosity if more than zero, activates some debug output (default: 0).
        \return a pointer to the received binary object, if an error occured or the function timedout the pointer is null. The buffer shall be freed with free().
     */
    std::vector<char> receive_byte_message(void *socket, unsigned int verbosity = 0);

    //! Sends a JSON message
    /*! \param socket the ØMQ socket.
        \param topic a string describing the topic of the message for PUB sockets, can be empty otherwise.
        \param json_message the Json::Value object to be sent.
        \param verbosity if more than zero, activates some debug output (default: 0).
        \return true if successfull false otherwise.
     */
    bool send_json_message(void *socket, \
                           std::string topic, \
                           Json::Value json_message, \
                           unsigned int verbosity = 0);

    //! Receives a JSON message
    /*! \param socket the ØMQ socket.
        \param verbosity if more than zero, activates some debug output (default: 0).
        \return the received JSON object, if an error occured or the function timedout an empty object
     */
    Json::Value receive_json_message(void *socket, unsigned int verbosity = 0);

    //! Receives a JSON message
    /*! \param socket the ØMQ socket.
        \param topic a string in which the topic will be written
        \param verbosity if more than zero, activates some debug output (default: 0).
        \return the received JSON object, if an error occured or the function timedout an empty object
     */
    Json::Value receive_json_message(void *socket, std::string &topic, unsigned int verbosity = 0);

    //! Receives a message and dumps it
    /*! \param socket the ØMQ socket.
        \return true if a message was received
    */
    bool discard_message(void *socket);
}

#endif
