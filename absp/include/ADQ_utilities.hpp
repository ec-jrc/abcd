#ifndef __ADQ_UTILITIES_HPP__
#define __ADQ_UTILITIES_HPP__

#include <vector>

#define LINUX
#include "ADQAPI.h"

#define WRITE_RED "\033[0;31m"
#define WRITE_YELLOW "\033[0;33m"
#define WRITE_NC "\033[0m"

#define CHECKZERO(f) \
{ \
    const auto retval = (f); \
    if (!(retval)) { \
        char time_buffer[BUFFER_SIZE]; \
        time_string(time_buffer, BUFFER_SIZE, NULL); \
        std::cout << '[' << time_buffer << "] ADQSDK "; \
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << " in: " << (#f); \
        std::cout << " (code: " << WRITE_YELLOW << retval << WRITE_NC << "); "; \
        std::cout << std::endl; \
    } \
}

#define CHECKNEGATIVE(f) \
{ \
    const auto retval = (f); \
    if ((retval) < 0) { \
        char time_buffer[BUFFER_SIZE]; \
        time_string(time_buffer, BUFFER_SIZE, NULL); \
        std::cout << '[' << time_buffer << "] ADQSDK "; \
        std::cout << WRITE_RED << "ERROR" << WRITE_NC << " in: " << (#f); \
        std::cout << " (code: " << WRITE_YELLOW << retval << WRITE_NC << "); "; \
        std::cout << std::endl; \
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
