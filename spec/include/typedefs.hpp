#ifndef __TYPEDEFS_HPP__
#define __TYPEDEFS_HPP__ 1

#include <unistd.h>
#include <string>
#include <chrono>
#include <map>
#include <cstdint>
#include <set>

#include "defaults.h"

#define counter_type double

extern "C" {
#include <zmq.h>

#include "histogram.h"
#include "histogram2D.h"
}

enum spectra_types {
    QLONG_SPECTRA = 0,
    QSHORT_SPECTRA = 1,
    BASELINE_SPECTRA = 2,
};

enum PSD_types {
    QTAIL_VS_ENERGY_PSD = 0,
    QSHORT_VS_ENERGY_PSD = 1,
    QLONG_VS_ENERGY_PSD = 2,
    BASELINE_VS_ENERGY_PSD = 3,
};

struct status
{
    std::string status_address = defaults_spec_status_address;
    std::string data_address = defaults_spec_data_address;
    std::string commands_address = defaults_spec_commands_address;
    std::string abcd_data_address = defaults_abcd_data_address;
    std::string subscription_topic = defaults_abcd_events_topic;

    std::string config_file;

    json_t *config = nullptr;

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

    enum spectra_types spectra_type;
    enum PSD_types PSD_type;
    bool PSD_normalize;

    std::set<unsigned int> active_channels;

    std::map<unsigned int, histogram_t*> histos_E;
    std::map<unsigned int, histogram2D_t*> histos_PSD;
    std::map<unsigned int, unsigned int> counts_partial;
    std::map<unsigned int, unsigned int> counts_total;

    unsigned int publish_timeout = defaults_spec_publish_timeout;

    bool time_decay_enabled = defaults_tofcalc_time_decay_enabled;
    double time_decay_tau = defaults_tofcalc_time_decay_tau;
    double time_decay_minimum = defaults_tofcalc_time_decay_minimum;

    bool disable_bidimensional_plot = false;
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
