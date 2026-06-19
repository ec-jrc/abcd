#ifndef __ADQ_UTILITIES_HPP__
#define __ADQ_UTILITIES_HPP__

#include <vector>
#include <memory>
#include <spdlog/spdlog.h>

#define LINUX
#include "ADQAPI.h"

extern std::shared_ptr<spdlog::logger> absp_logger_console;
extern std::shared_ptr<spdlog::logger> absp_logger_error;

#define CHECKZERO(f) \
{ \
    const auto retval = (f); \
    if (!(retval)) { \
        char error_string[512]; \
        ADQControlUnit_GetLastFailedDeviceErrorWithText(adq_cu_ptr, error_string); \
        absp_logger_error->error("ADQSDK ERROR in: {} (code: {}); text: {}", (#f), retval, error_string); \
    } \
}

#define CHECKNEGATIVE(f) \
{ \
    const auto retval = (f); \
    if ((retval) < 0) { \
        absp_logger_error->error("ADQSDK ERROR in: {} (code: {})", (#f), retval); \
    } \
}

#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

template<class T> unsigned int IndexOfClosest(T value, std::vector<T> array) {
    double min_closeness = fabs(value - array[0]);
    unsigned int index_closest = 0;

    for (unsigned int index = 1; index < array.size(); index++) {
        const double this_closeness = fabs(value - array[index]);
        if (min_closeness > this_closeness) {
            min_closeness = this_closeness;
            index_closest = index;
        }
    }

    return index_closest;
}

#endif
