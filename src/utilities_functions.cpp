// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <ctime>
// For memcpy()
#include <cstring>
#include <cerrno>
#include <string>
#include <fstream>

#include "utilities_functions.hpp"

//! Prints the local date and time in ISO 8601 format
std::string utilities_functions::time_string()
{
    std::time_t raw_time;
    std::time(&raw_time);

    const struct tm *time_info = std::localtime(&raw_time);

    // I HATE to use static buffers, on a modern programming language they should be not necessary...
    char buffer[sizeof("[2011-10-08T07:07:09Z+0100]")];

    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S%z", time_info);

    return std::string(buffer);
}

//! Prints the local date and time in ISO 8601 format
std::string utilities_functions::time_string(std::tm time_info)
{
    // I HATE to use static buffers, on a modern programming language they should be not necessary...
    char buffer[sizeof("[2011-10-08T07:07:09Z+0100]")];

    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S%z", &time_info);

    return std::string(buffer);
}

//! Converts a ZMQ address of the type tcp://*:16180 to tcp://127.0.0.1:16180
std::string utilities_functions::address_bind_to_connect(std::string address, std::string ip)
{
    std::size_t found = address.find('*');

    if (found != std::string::npos)
    {
        address.erase(found, 1);
        address.insert(found, ip);
    }

    return address;
}
