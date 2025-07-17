#ifndef __UTILITIES_FUNCTIONS_H__
#define __UTILITIES_FUNCTIONS_H__ 1

#include <math.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

//! Prints the local date and time in ISO 8601 format
extern inline
int time_string(char *output_buffer, size_t N, const struct tm *time_info)
{
    if (time_info)
    {
        strftime(output_buffer, N, "%Y-%m-%dT%H:%M:%S%z", time_info);
    }
    else
    {
        time_t raw_time;
        time(&raw_time);

        struct tm this_time_info;

        localtime_r(&raw_time, &this_time_info);

        strftime(output_buffer, N, "%Y-%m-%dT%H:%M:%S%z", &this_time_info);
    }

    return EXIT_SUCCESS;
}

//! Converts a ZMQ address of the type tcp://*:16180 to tcp://127.0.0.1:16180
extern inline
int address_bind_to_connect(char *output_buffer, size_t N, const char *address, const char *ip)
{
    const size_t length_address = strlen(address);
    const size_t length_ip = strlen(ip);
    const size_t length_final = length_address + length_ip - 1;

    const char *position = strchr(address, '*');

    if (position && length_final < N)
    {
        const size_t offset = position - address;

        memcpy(output_buffer, address, offset);
        memcpy(output_buffer + offset, ip, length_ip);
        memcpy(output_buffer + offset + length_ip, address + offset + 1, length_address - offset - 1);
        output_buffer[length_final] = '\0';
    }

    return EXIT_SUCCESS;
}

//! Converts an integer representing a size in bytes to a string with the proper unit
extern inline
int file_size_to_human(char *output_buffer, size_t N, const size_t size) {
    const char *exponents[] = {"B", "kiB", "MiB", "GiB", "TiB", "PiB"};

    if (size == 0)
    {
        strncpy(output_buffer, "0 B", N);
    }
    else
    {
        const int exponent = floor(log(size) / log(1024));
        const float mantissa = size / pow(1024, exponent);

        const unsigned int result = snprintf(output_buffer, N, "%.0f %s", mantissa, exponents[exponent]);

        if (result >= N)
        {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

/// @brief Reverse strstr(): searches small_string in big_string
/// @param big_string Bigger string that might contain small_string
/// @param small_string Smaller string that might be contained in big_string
/// @return Pointer to the identified position in the big_string
extern inline
char *rstrstr(char *big_string, char *small_string)
{
    size_t big_string_len = strlen(big_string);
    size_t small_string_len = strlen(small_string);

    if (small_string_len > big_string_len)
    {
        return NULL;
    }
    for (char *s = big_string + big_string_len - small_string_len; s >= big_string; --s)
    {
        if (strncmp(s, small_string, small_string_len) == 0)
        {
            return s;
        }
    }

    return NULL;
}

#endif
