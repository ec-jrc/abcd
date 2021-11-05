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
#include <jansson.h>

#include "analysis_functions.h"
}

struct status
{
    std::string status_address = defaults_waan_status_address;
    std::string data_input_address = defaults_abcd_data_address;
    std::string data_output_address = defaults_waan_data_address;
    std::string commands_address = defaults_waan_commands_address;
    std::string subscription_topic = defaults_abcd_events_topic;

    std::string config_file;

    double ns_per_sample;

    json_t *config = nullptr;

    void *context = nullptr;
    void *status_socket = nullptr;
    void *data_input_socket = nullptr;
    void *data_output_socket = nullptr;
    void *commands_socket = nullptr;

    unsigned int verbosity = 0;
    unsigned long int status_msg_ID = 0;
    unsigned long int waveforms_msg_ID = 0;
    unsigned long int events_msg_ID = 0;

    std::chrono::time_point<std::chrono::system_clock> system_start;
    std::chrono::time_point<std::chrono::system_clock> last_publication;

    bool forward_waveforms;
    bool enable_additional;

    std::set<unsigned int> active_channels;
    std::set<unsigned int> disabled_channels;

    std::map<unsigned int, void*> dl_timestamp_handles;
    std::map<unsigned int, union WA_init_union> channels_timestamp_init;
    std::map<unsigned int, union WA_timestamp_union> channels_timestamp_analysis;
    std::map<unsigned int, union WA_close_union> channels_timestamp_close;
    std::map<unsigned int, void*> dl_energy_handles;
    std::map<unsigned int, union WA_init_union> channels_energy_init;
    std::map<unsigned int, union WA_energy_union> channels_energy_analysis;
    std::map<unsigned int, union WA_close_union> channels_energy_close;
    std::map<unsigned int, void*> channels_timestamp_user_config;
    std::map<unsigned int, void*> channels_energy_user_config;

    std::map<unsigned int, unsigned int> partial_counts;

    unsigned int publish_period = defaults_waan_publish_period;
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
