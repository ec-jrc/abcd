#ifndef __TYPEDEFS_HPP__
#define __TYPEDEFS_HPP__ 1

#include <unistd.h>
#include <string>
#include <chrono>
#include <map>
#include <cstdint>
#include <set>

#include "defaults.h"

#define counter_type uint64_t

extern "C" {
#include <zmq.h>

#include "histogram.h"
#include "histogram2D.h"
}

struct status
{
    std::string status_address = defaults_pqrs_status_address;
    std::string data_address = defaults_pqrs_data_address;
    std::string commands_address = defaults_pqrs_commands_address;
    std::string abcd_data_address = defaults_abcd_data_address;
    std::string subscription_topic = defaults_abcd_events_topic;

    void *context = nullptr;
    void *status_socket = nullptr;
    void *data_socket = nullptr;
    void *commands_socket = nullptr;
    void *abcd_data_socket = nullptr;

    unsigned int verbosity = 0;
    unsigned long int status_msg_ID = 0;
    unsigned long int data_msg_ID = 0;

    std::chrono::time_point<std::chrono::system_clock> system_start;
    std::chrono::time_point<std::chrono::system_clock> last_publication;

    std::set<unsigned int> active_channels;

    std::map<unsigned int, histogram_t*> histos_E;
    std::map<unsigned int, histogram2D_t*> histos_PSD;
    std::map<unsigned int, unsigned int> partial_counts;

    unsigned int publish_timeout = defaults_pqrs_publish_timeout;
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
