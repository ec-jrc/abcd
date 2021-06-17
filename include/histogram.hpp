#ifndef __HISTOGRAM_HPP__
#define __HISTOGRAM_HPP__

#include <cmath>
#include <cstdint>
#include <vector>
#include <iostream>

#include <json/json.h>

#include "utilities_functions.hpp"
#include "defaults.h"

template <class data_type>
class histogram
{
    public:
    unsigned int verbosity = defaults_pqrs_verbosity;

    unsigned int bins;
    data_type min;
    data_type max;
    data_type delta;

    double bin_width;

    Json::Value config;

    std::vector<uint64_t> histo;

    histogram() {};

    inline explicit histogram(unsigned int Bins,
                              data_type Min,
                              data_type Max,
                              unsigned int Verbosity = defaults_pqrs_verbosity)
    {
        reconfigure(Bins, Min, Max, Verbosity);
    }

    inline explicit histogram(Json::Value new_config)
    {
        reconfigure(new_config);
    }

    inline ~histogram() {};

    inline void reconfigure(unsigned int Bins,
                            data_type Min,
                            data_type Max,
                            unsigned int Verbosity = defaults_pqrs_verbosity)
    {
        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "histogram::reconfigure() ";
            std::cout << std::endl;
        }

        Json::Value new_config;

        new_config["verbosity"] = Verbosity;
        new_config["bins"] = Bins;
        new_config["min"] = Min;
        new_config["max"] = Max;

        reconfigure(new_config);
    }

    inline void reconfigure(Json::Value new_config)
    {
        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "histogram::reconfigure() ";
            std::cout << std::endl;
        }

        try
        {
            verbosity = new_config["verbosity"].asUInt();
            config["verbosity"] = verbosity;
        }
        // Can not use LogicError to keep retrocompatibility with Ubuntu 14
        //catch (Json::LogicError &error) { std::cout << "ERROR: verbosity " << error.what() << std::endl; }
        catch (...)
        {
            std::cout << "ERROR: verbosity; " << std::endl;
        }

        try
        {
            bins = new_config["bins"].asUInt();
            config["bins"] = bins;
        }
        //catch (Json::LogicError &error) { std::cout << "ERROR: bins " << error.what() << std::endl; }
        catch (...)
        {
            std::cout << "ERROR: bins; " << std::endl;
        }

        try
        {
            min = static_cast<data_type>(new_config["min"].asDouble());
            config["min"] = min;
        }
        //catch (Json::LogicError &error) { std::cout << "ERROR: min " << error.what() << std::endl; }
        catch (...)
        {
            std::cout << "ERROR: min; " << std::endl;
        }

        try
        {
            max = static_cast<data_type>(new_config["max"].asDouble());
            config["max"] = max;
        }
        //catch (Json::LogicError &error) { std::cout << "ERROR: max " << error.what() << std::endl; }
        catch (...)
        {
            std::cout << "ERROR: max; " << std::endl;
        }

        bin_width = static_cast<double>(max - min) / bins;
        config["bin_width"] = bin_width;

        delta = max - min;

        histo.clear();
        histo.resize(bins);
    }

    inline void reset(void)
    {
        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "histogram::reset() ";
            std::cout << std::endl;
        }

        histo.clear();
        histo.resize(bins);
    }

    inline void fill(data_type value)
    {
        const data_type offset_value = value - min;

        const double norm_value = static_cast<double>(offset_value / bin_width);
        const long int bin = static_cast<long int>(std::floor(norm_value));

        // I wonder if this is going to be faster,
        // because we can use only integer arithmetics if the templace class is an integer
        // After some tests it seems equivalent so I will leave the float calculation
        //const long int bin = (bins * offset_value) / delta;

        if (0 <= bin && bin < bins)
        {
            histo[bin] += 1;
        }

        if (verbosity > 1)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "histogram::fill() ";
            std::cout << "value: " << value << "; ";
            //std::cout << "norm_value: " << norm_value << "; ";
            std::cout << "bin: " << bin << "; ";
            std::cout << std::endl;
        }
    }

    inline Json::Value to_json() const
    {
        Json::Value counts_array;

        for(auto &counts: histo)
        {
            counts_array.append(Json::Value::UInt64(counts));
        }

        Json::Value output;

        output["config"] = config;
        output["histo"] = counts_array;

        return output;
    }
};

template <class data_type_x, class data_type_y = data_type_x>
class histogram2D
{
    public:
    unsigned int verbosity = defaults_pqrs_verbosity;

    unsigned int bins_x;
    data_type_x min_x;
    data_type_x max_x;
    data_type_x delta_x;

    unsigned int bins_y;
    data_type_y min_y;
    data_type_y max_y;
    data_type_y delta_y;

    double bin_width_x;
    double bin_width_y;

    Json::Value config;

    std::vector<std::vector<uint64_t>> histo;

    histogram2D() {};

    inline explicit histogram2D(unsigned int Bins_x,
                                data_type_x Min_x,
                                data_type_x Max_x,
                                unsigned int Bins_y,
                                data_type_y Min_y,
                                data_type_y Max_y,
                                unsigned int Verbosity = defaults_pqrs_verbosity)
    {
        reconfigure(Bins_x, Min_x, Max_x, Bins_y, Min_y, Max_y, Verbosity);
    }

    inline explicit histogram2D(Json::Value new_config)
    {
        reconfigure(new_config);
    }

    inline ~histogram2D() {};

    inline void reconfigure(unsigned int Bins_x,
                            data_type_x Min_x,
                            data_type_x Max_x,
                            unsigned int Bins_y,
                            data_type_y Min_y,
                            data_type_y Max_y,
                            unsigned int Verbosity = defaults_pqrs_verbosity)
    {
        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "histogram2D::reconfigure() ";
            std::cout << std::endl;
        }

        Json::Value new_config;

        new_config["verbosity"] = Verbosity;
        new_config["bins_x"] = Bins_x;
        new_config["min_x"] = Min_x;
        new_config["max_x"] = Max_x;
        new_config["bins_y"] = Bins_y;
        new_config["min_y"] = Min_y;
        new_config["max_y"] = Max_y;

        reconfigure(new_config);
    }

    inline void reconfigure(Json::Value new_config)
    {
        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "histogram2D::reconfigure() ";
            std::cout << std::endl;
        }

        try
        {
            verbosity = new_config["verbosity"].asUInt();
            config["verbosity"] = verbosity;
        }
        //catch (Json::LogicError &error) { std::cout << "ERROR: verbosity " << error.what() << std::endl; }
        catch (...)
        {
            std::cout << "ERROR: verbosity; " << std::endl;
        }

        try
        {
            bins_x = new_config["bins_x"].asUInt();
            config["bins_x"] = bins_x;
        }
        //catch (Json::LogicError &error) { std::cout << "ERROR: bins_x " << error.what() << std::endl; }
        catch (...)
        {
            std::cout << "ERROR: bins_x; " << std::endl;
        }

        try
        {
            min_x = static_cast<data_type_x>(new_config["min_x"].asDouble());
            config["min_x"] = min_x;
        }
        //catch (Json::LogicError &error) { std::cout << "ERROR: min_x " << error.what() << std::endl; }
        catch (...)
        {
            std::cout << "ERROR: min_x; " << std::endl;
        }

        try
        {
            max_x = static_cast<data_type_x>(new_config["max_x"].asDouble());
            config["max_x"] = max_x;
        }
        //catch (Json::LogicError &error) { std::cout << "ERROR: max_x " << error.what() << std::endl; }
        catch (...)
        {
            std::cout << "ERROR: max_x; " << std::endl;
        }

        try
        {
            bins_y = new_config["bins_y"].asUInt();
            config["bins_y"] = bins_y;
        }
        //catch (Json::LogicError &error) { std::cout << "ERROR: bins_y " << error.what() << std::endl; }
        catch (...)
        {
            std::cout << "ERROR: bins_y; " << std::endl;
        }

        try
        {
            min_y = static_cast<data_type_y>(new_config["min_y"].asDouble());
            config["min_y"] = min_y;
        }
        //catch (Json::LogicError &error) { std::cout << "ERROR: min_y " << error.what() << std::endl; }
        catch (...)
        {
            std::cout << "ERROR: min_y; " << std::endl;
        }

        try
        {
            max_y = static_cast<data_type_y>(new_config["max_y"].asDouble());
            config["max_y"] = max_y;
        }
        //catch (Json::LogicError &error) { std::cout << "ERROR: max_y " << error.what() << std::endl; }
        catch (...)
        {
            std::cout << "ERROR: max_y; " << std::endl;
        }

        bin_width_x = static_cast<double>(max_x - min_x) / bins_x;
        bin_width_y = static_cast<double>(max_y - min_y) / bins_y;
        config["bin_width_x"] = bin_width_x;
        config["bin_width_y"] = bin_width_y;

        delta_x = max_x - min_x;
        delta_y = max_y - min_y;

        histo.clear();
        histo.resize(bins_x, std::vector<uint64_t>(bins_y));
    }

    inline void reset(void)
    {
        if (verbosity > 0)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "histogram2D::reset() ";
            std::cout << std::endl;
        }

        histo.clear();
        histo.resize(bins_x, std::vector<uint64_t>(bins_y));
    }

    inline void fill(data_type_x value_x, data_type_y value_y)
    {
        const data_type_x offset_value_x = value_x - min_x;
        const double norm_value_x = static_cast<double>(offset_value_x / bin_width_x);
        const long int bin_x = static_cast<long int>(std::floor(norm_value_x));

        //const long int bin_x = (bins_x * offset_value_x) / delta_x;

        const data_type_y offset_value_y = value_y - min_y;
        const double norm_value_y = static_cast<double>(offset_value_y / bin_width_y);
        const long int bin_y = static_cast<long int>(std::floor(norm_value_y));

        //const long int bin_y = (bins_y * offset_value_y) / delta_y;

        if (0 <= bin_x && bin_x < bins_x && 0 <= bin_y && bin_y < bins_y)
        {
            histo[bin_x][bin_y] += 1;
        }

        if (verbosity > 1)
        {
            std::cout << '[' << utilities_functions::time_string() << "] ";
            std::cout << "histogram2D::fill() ";
            std::cout << "value_x: " << value_x << "; ";
            std::cout << "bin_x: " << bin_x << "; ";
            std::cout << "value_y: " << value_y << "; ";
            std::cout << "bin_y: " << bin_y << "; ";
            std::cout << std::endl;
        }
    }

    inline Json::Value to_json() const
    {
        Json::Value counts_array;

        for (auto &sub_vector: histo)
        {
            Json::Value sub_array;

            for (auto &counts: sub_vector)
            {
                sub_array.append(Json::Value::UInt64(counts));
            }

            counts_array.append(sub_array);
        }

        Json::Value output;

        output["config"] = config;
        output["histo2D"] = counts_array;

        return output;
    }
};

#endif
