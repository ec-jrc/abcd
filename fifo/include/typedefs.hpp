#ifndef __TYPEDEFS_HPP__
#define __TYPEDEFS_HPP__ 1

#include <unistd.h>
#include <string>
#include <chrono>
#include <map>
#include <cstdint>
#include <set>

#include <zmq.h>

#include "defaults.h"
#include "binary_fifo.hpp"

struct status
{
    std::string status_address = defaults_fifo_status_address;
    std::string reply_address = defaults_fifo_reply_address;
    std::string abcd_data_address = defaults_abcd_data_address;
    std::string subscription_topic = defaults_abcd_events_topic;

    std::string config_file;

    double ns_per_sample;

    void *context = nullptr;
    void *status_socket = nullptr;
    void *reply_socket = nullptr;
    void *abcd_data_socket = nullptr;

    unsigned int verbosity = 0;
    unsigned long int status_msg_ID = 0;
    unsigned long int reply_msg_ID = 0;
    std::chrono::time_point<std::chrono::system_clock> last_publication;

    binary_fifo::binary_fifo fifo;

    binary_fifo::long_int expiration_time = defaults_fifo_expiration_time;
};

struct state
{
    unsigned int ID;
    const char* description;
    struct state (*act)(struct status&);
};

inline bool operator==(struct state a, struct state b)
{
    return a.ID == b.ID;
}

typedef struct status status;
typedef struct state state;
typedef state (*action)(status);

#endif
