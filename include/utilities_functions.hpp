#ifndef __UTILITIES_FUNCTIONS_HPP__
#define __UTILITIES_FUNCTIONS_HPP__ 1

#include <map>
#include <vector>
#include <string>
#include <algorithm>

#include <json/json.h>

namespace utilities_functions {

    //! Prints the local date and time in ISO 8601 format
    std::string time_string();
    std::string time_string(std::tm time_info);

    //! Converts a ZMQ address of the type tcp://*:16180 to tcp://127.0.0.1:16180
    std::string address_bind_to_connect(std::string address, std::string ip);

    //! Get key from a dictionary pair
    template <class key_type, class item_type>
    inline key_type get_key(const std::pair<key_type, item_type> &pair)
    {
        return pair.first;
    }

    //! Get list of keys from a dictionary. Why is it so complicated? I HATE C++!!!
    template <class key_type, class item_type>
    inline std::vector<key_type> get_keys(std::map<key_type, item_type> dict)
    {
        std::vector<key_type> keys(dict.size());
        std::transform(dict.begin(), dict.end(),
                       keys.begin(),
                       get_key<key_type, item_type>);

        return keys;
    }

}

#endif
