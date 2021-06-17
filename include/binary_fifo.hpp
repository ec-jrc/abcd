#ifndef __DATA_FIFO_HPP__
#define __DATA_FIFO_HPP__ 1

#include <chrono>
#include <deque>
#include <vector>
#include <functional>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstdint>

namespace binary_fifo {
    // Definition of a standard long int type, in order to be able to change it easily
    typedef int64_t long_int;
    typedef uint64_t long_uint;

    // Some useful typedefs to reduce coding verbosity.
    // TODO: The sysyem_clock resolution is nanoseconds, convert it to seconds
    typedef std::chrono::system_clock clock;
    typedef std::chrono::duration<long_int, std::ratio<1, 1000000000ULL> > nanoseconds;
    typedef std::chrono::time_point <clock, nanoseconds> time_point;
    typedef std::vector <uint8_t> binary_data;

    class datum
    {
        public:
            binary_data data;
            time_point timestamp;

            datum(binary_data Buffer, time_point Timestamp = clock::now()) : \
                data(Buffer), timestamp(Timestamp) {};
            ~datum() {};
    };

    inline bool operator<(datum a, datum b)
    {
        return a.timestamp < b.timestamp;
    }

    inline bool operator>(datum a, datum b)
    {
        return a.timestamp > b.timestamp;
    }

    class binary_fifo
    {
        public:
            nanoseconds expiration_time;

            std::tm epoch_struct;
            time_point epoch;

            std::deque<datum> buffer;
            
            binary_fifo(nanoseconds Expiration_Time = nanoseconds()) : \
                expiration_time(Expiration_Time)
            {
                epoch_struct.tm_sec = 0;
                epoch_struct.tm_min = 0;
                epoch_struct.tm_hour = 0;
                // Day of the month
                epoch_struct.tm_mday = 1;
                // Months since January
                epoch_struct.tm_mon = 0;
                // Years after 1900
                epoch_struct.tm_year = 70;
                // Daylight saving time information not available
                epoch_struct.tm_isdst = -1;

                std::time_t epoch_time = std::mktime(&epoch_struct);
                epoch = clock::from_time_t(epoch_time);
            };

            ~binary_fifo() {};

            inline void set_expiration_time(long_int new_interval)
            {
                expiration_time = nanoseconds(new_interval);
            }

            inline long_int get_expiration_time() const
            {
                return expiration_time.count();
            }

            inline void update()
            {
                if (expiration_time > nanoseconds(0))
                {
                    const time_point now = std::chrono::system_clock::now();

                    for (auto it = buffer.begin(); it != buffer.end(); it++)
                    {
                        const nanoseconds time_difference = now - it->timestamp;

                        if (time_difference > expiration_time)
                        {
                            buffer.pop_front();
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }

            
            inline void push(binary_data data)
            {
                buffer.emplace_back(data, clock::now());

                //update();
            }

            inline void push_vector(std::vector<binary_data> data)
            {
                for (binary_data &these_data: data)
                {
                    buffer.emplace_back(these_data, clock::now());
                }

                //update();
            }
            
            inline typename std::deque<datum>::iterator _find_first_younger(time_point timestamp)
            {
                //update();

                for (auto it = buffer.begin(); it != buffer.end(); it++)
                {
                    if (timestamp <= (it->timestamp))
                    {
                        return it;
                    }
                }

                return buffer.end();
            }

            template <class returned_type>
            inline returned_type reduce( \
                                        std::function<returned_type (returned_type, binary_data)> fn, \
                                        time_point begin = clock::now() + nanoseconds(1000000000ULL), \
                                        time_point end = clock::now(), \
                                        returned_type initial_aggregate = returned_type())
            {
                //update();

                typename std::deque<datum>::iterator start = buffer.begin();

                if (begin < end)
                {
                    start = binary_fifo::_find_first_younger(begin);
                }

                returned_type aggregate = initial_aggregate;

                for (auto it = start; it != buffer.end(); it++)
                {
                    if (end > (it->timestamp))
                    {
                        aggregate = fn(aggregate, it->data);
                    }
                    else
                    {
                        return aggregate;
                    }
                }

                return aggregate;
            }

            template <class returned_type>
            inline returned_type reduce( \
                                        returned_type (*fn)(returned_type, binary_data), \
                                        time_point begin = clock::now() + nanoseconds(1000000000ULL), \
                                        time_point end = clock::now(), \
                                        returned_type initial_aggregate = returned_type())
            {
                std::function<returned_type(returned_type, binary_data)> func(fn);
                return reduce(func, begin, end, initial_aggregate);
            }

            inline std::vector<binary_data> filter(std::function<bool (binary_data)> fn, \
                                                   time_point begin = clock::now() + nanoseconds(1000000000ULL), \
                                                   time_point end = clock::now())
            {
                //update();

                typename std::deque<datum>::iterator start = buffer.begin();

                if (begin < end)
                {
                    start = binary_fifo::_find_first_younger(begin);
                }

                std::vector<binary_data> result;

                for (auto it = start; it != buffer.end(); it++)
                {
                    if (end > (it->timestamp))
                    {
                        if(fn(it->data))
                        {
                            result.push_back(it->data);
                        }
                    }
                    else
                    {
                        return result;
                    }
                }

                return result;
            }

            inline std::vector<binary_data> filter(bool (*fn)(binary_data), \
                                                   time_point begin = clock::now() + nanoseconds(1000000000ULL), \
                                                   time_point end = clock::now())
            {
                std::function<bool(binary_data)> func(fn);
                return filter(func, begin, end);
            }

            template <class returned_type>
            inline std::vector<returned_type> map(std::function<returned_type (binary_data)> fn, \
                                                  time_point begin = clock::now() + nanoseconds(1000000000ULL), \
                                                  time_point end = clock::now())
            {
                //update();

                typename std::deque<datum>::iterator start = buffer.begin();

                if (begin < end)
                {
                    start = binary_fifo::_find_first_younger(begin);
                }

                std::vector<binary_data> result;

                for (auto it = start; it != buffer.end(); it++)
                {
                    if (end > (it->timestamp))
                    {
                        result.push_back(fn(it->data));
                    }
                    else
                    {
                        return result;
                    }
                }

                return result;
            }

            template <class returned_type>
            inline std::vector<returned_type> map(returned_type (*fn)(binary_data), \
                                                  time_point begin = clock::now() + nanoseconds(1000000000ULL), \
                                                  time_point end = clock::now())
            {
                std::function<returned_type(binary_data)> func(fn);
                return map(func, begin, end);
            }

            inline unsigned int count()
            {
                std::function<unsigned int(unsigned int, binary_data)> sum = [](unsigned int a, binary_data) -> unsigned int {return a + 1;};
                return reduce(sum);
            }

            inline unsigned int size()
            {
                std::function<size_t(size_t, binary_data)> sum = [](size_t a, binary_data data) -> unsigned int {return a + (data.size() * sizeof(uint8_t));};
                return reduce(sum);
            }

            inline void sort_buffer()
            {
                std::sort(buffer.begin(), buffer.end());
            };

            inline bool save_to_file(std::string filename, \
                                     time_point begin = clock::now() + nanoseconds(1000000000ULL), \
                                     time_point end = clock::now())
            {
                //update();

                std::ofstream out_file;
                out_file.open(filename, std::ios::out | std::ios::binary);

                if (!out_file.is_open())
                {
                    return false;
                }

                typename std::deque<datum>::iterator start = buffer.begin();

                if (begin < end)
                {
                    start = binary_fifo::_find_first_younger(begin);
                }

                for (auto it = start; it != buffer.end(); it++)
                {
                    if (end > (it->timestamp))
                    {
                        const nanoseconds time_since_epoch = it->timestamp - epoch;
                        const long_int since = time_since_epoch.count();

                        const binary_data data = it->data;
                        const long_uint size = data.size() * sizeof(uint8_t);

                        const char *since_pointer = reinterpret_cast<const char*>(&since);
                        const char *size_pointer = reinterpret_cast<const char*>(&size);
                        const char *data_pointer = reinterpret_cast<const char*>(data.data());

                        out_file.write(since_pointer, sizeof(long_int));
                        out_file.write(size_pointer, sizeof(long_uint));
                        out_file.write(data_pointer, data.size() * sizeof(uint8_t));
                    }
                    else
                    {
                        break;
                    }
                }

                out_file.close();

                return true;
            }

            inline void load_from_file(std::string filename)
            {
                long_int since;
                long_uint size;

                std::ifstream in_file;
                in_file.open(filename, std::ios::in | std::ios::binary);

                while (in_file.good())
                {
                    in_file.read(reinterpret_cast<char*>(&since), sizeof(long_int));
                    in_file.read(reinterpret_cast<char*>(&size), sizeof(long_uint));

                    binary_data data(size);

                    in_file.read(reinterpret_cast<char*>(data.data()), sizeof(uint8_t) * size);

                    const nanoseconds time_since_epoch = nanoseconds(since);

                    const time_point timestamp = epoch + time_since_epoch;

                    buffer.emplace_back(data, timestamp);
                }

                in_file.close();

                sort_buffer();

                //update();
            }

            inline std::vector<binary_data> get_data(time_point begin = clock::now() + nanoseconds(1000000000ULL), \
                                              time_point end = clock::now())
            {
                std::function<binary_data(binary_data)> identity = [](binary_data a) -> binary_data {return a;};
                return map(identity, begin, end);
            }

            inline std::vector<long_int> get_timestamps_from_epoch(time_point this_epoch, \
                                                        time_point begin = clock::now() + nanoseconds(1000000000ULL), \
                                                        time_point end = clock::now())
            {
                //update();

                typename std::deque<datum>::iterator start = buffer.begin();

                if (begin < end)
                {
                    start = binary_fifo::_find_first_younger(begin);
                }

                std::vector<long_int> result;

                for (auto it = start; it != buffer.end(); it++)
                {
                    if (end > (it->timestamp))
                    {
                        const nanoseconds time_since_epoch = it->timestamp - this_epoch;
                        const long_int since = time_since_epoch.count();

                        result.push_back(since);
                    }
                    else
                    {
                        return result;
                    }
                }

                return result;
            }

            inline std::vector<long_int> get_timestamps(time_point begin = clock::now() + nanoseconds(1000000000ULL), \
                                                        time_point end = clock::now())
            {
                return get_timestamps_from_epoch(epoch, begin, end);
            }
    };
}

#endif
