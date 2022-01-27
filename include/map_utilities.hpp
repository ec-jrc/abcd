#ifndef __MAP_UTILITIES_HPP__
#define __MAP_UTILITIES_HPP__ 1

#include <map>
#include <vector>
#include <algorithm>

namespace map_utilities {

    //! Get key from a dictionary pair
    template <typename key_type, typename item_type>
    inline key_type get_key(const std::pair<key_type, item_type> &pair)
    {
        return pair.first;
    }

    //! Get list of keys from a dictionary. Why is it so complicated? I HATE C++!!!
    template <typename key_type, typename item_type>
    inline std::vector<key_type> get_keys(std::map<key_type, item_type> dict)
    {
        std::vector<key_type> keys(dict.size());
        std::transform(dict.begin(), dict.end(),
                       keys.begin(),
                       get_key<key_type, item_type>);

        return keys;
    }

    // Why does C++ needs to be so complicated?!
    template <typename key_type, typename item_type>
    inline typename std::map<key_type, item_type>::iterator
    find_item(std::map<key_type, item_type> dict, item_type item) {
        return std::find_if(dict.begin(), dict.end(),
                            [item](const std::pair<key_type, item_type> &pair)
                            { return pair.second == item; });
    }

}

#endif
