#ifndef __TYPEDEFS_HPP__
#define __TYPEDEFS_HPP__ 1

#include <unistd.h>
#include <string>
#include <chrono>
#include <map>
#include <cstdint>
#include <set>

#include <zmq.h>
#include <gsl/gsl_rng.h>

#include "defaults.h"
#include "binary_fifo.hpp"

#define counter_type double

extern "C" {
#include "histogram.h"
#include "fit_functions.h"
}

struct background_parameters
{
    bool enable;
    int iterations;
    int window_direction;
    bool enable_smooth;
    int smooth;
    int order;
    bool compton;
};

struct status
{
    gsl_rng *rng = nullptr;

    std::string status_address = defaults_califo_status_address;
    std::string data_address = defaults_califo_data_address;
    std::string abcd_data_address = defaults_abcd_data_address;
    std::string subscription_topic = defaults_abcd_events_topic;

    std::string config_file;
    json_t *config;

    void *context = nullptr;
    void *status_socket = nullptr;
    void *data_socket = nullptr;
    void *abcd_data_socket = nullptr;

    unsigned int verbosity = 0;
    unsigned long int status_msg_ID = 0;
    unsigned long int data_msg_ID = 0;

    bool publish_fit_events = defaults_califo_publish_fit_events;
    std::chrono::time_point<std::chrono::system_clock> system_start;
    std::chrono::time_point<std::chrono::system_clock> last_publication;
    std::chrono::time_point<std::chrono::system_clock> last_fit;

    std::set<unsigned int> enabled_channels;

    std::map<unsigned int, struct peak_parameters> peaks;
    std::map<unsigned int, double> peak_tolerances;
    std::map<unsigned int, struct peak_parameters> last_fitted_peaks;
    std::map<unsigned int, double> last_scale_factors;
    std::map<unsigned int, histogram_t*> histos_E;

    std::map<unsigned int, struct background_parameters> background_estimates;

    std::map<unsigned int, binary_fifo::binary_fifo> fifoes;
    binary_fifo::long_int expiration_time = defaults_califo_expiration_time;

    unsigned int accumulation_time = defaults_califo_accumulation_time;
    unsigned int publish_timeout = defaults_califo_publish_timeout;
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
