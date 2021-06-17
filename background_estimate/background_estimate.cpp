// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/*
 *
 */

// For getopt()
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <list>
#include <cmath>
#include <stdexcept>

//#include "Rtypes.h"
//#include "TSpectrum.h"
#include "background.hpp"

#define NUMBER_OF_ITERATIONS 10
#define WINDOW_DIRECTION background::kBackDecreasingWindow
#define FILTER_ORDER background::kBackOrder2
#define DO_SMOOTHING false
#define SMOOTHING_WINDOW background::kBackSmoothing3
#define COMPTON_ESTIMATION false
#define VERBOSE false
#define MIN_X_SELECTION 0
#define MAX_X_SELECTION -1

void print_usage(char *name)
{
    std::cout << "Usage: " << name << " [options] <data_file> [<data_file> [...]]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "\t-n <number>: Number of iterations (default: " << NUMBER_OF_ITERATIONS << ")" << std::endl;
    std::cout << "\t-i: Set window direction to increasing (default: decreasing)" << std::endl;
    std::cout << "\t-o <order>: Filter order (default: 2, possible values: 2, 4, 6, 8)" << std::endl;
    std::cout << "\t-w <width>: Smoothing window, activates smoothing (default: deactivated, possible values: 3, 5, 7, 9, 11, 13, 15)" << std::endl;
    std::cout << "\t-c: Activates compton estimation (default: deactivated)" << std::endl;
    std::cout << "\t-v: Activates verbose output (default: deactivated)" << std::endl;
    std::cout << "\t-x: Minimum x value, for selecting range (default: " << MIN_X_SELECTION << ")" << std::endl;
    std::cout << "\t-X: Maximum x value, for selecting range (default: " << MAX_X_SELECTION << ", -1 to disable max filtering)" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        print_usage(argv[0]);

        return EXIT_SUCCESS;
    }

    int number_of_iterations = NUMBER_OF_ITERATIONS;
    int window_direction = WINDOW_DIRECTION;
    int filter_order = FILTER_ORDER;
    bool do_smoothing = DO_SMOOTHING;
    int smoothing_window = SMOOTHING_WINDOW;
    int compton_estimation = COMPTON_ESTIMATION;
    bool verbose = false;

    double min_x_selection = MIN_X_SELECTION;
    double max_x_selection = MAX_X_SELECTION;

    int c = 0;
    while ((c = getopt(argc, argv, "hn:io:w:cvx:X:")) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'n':
                try
                {
                    number_of_iterations = std::stoi(optarg);
                }
                catch (std::logic_error &e)
                { }
                break;
            case 'i':
                window_direction = background::kBackIncreasingWindow;
                break;
            case 'o':
                try
                {
                    switch (std::stoi(optarg)) {
                        case 2:
                            filter_order = background::kBackOrder2;
                            break;
                        case 4:
                            filter_order = background::kBackOrder4;
                            break;
                        case 6:
                            filter_order = background::kBackOrder6;
                            break;
                        case 8:
                            filter_order = background::kBackOrder8;
                            break;
                    }
                }
                catch (std::logic_error &e)
                { }
                break;
            case 'w':
                do_smoothing = true;
                try
                {
                    switch (std::stoi(optarg)) {
                        case 5:
                            smoothing_window = background::kBackSmoothing5;
                            break;
                        case 7:
                            smoothing_window = background::kBackSmoothing7;
                            break;
                        case 9:
                            smoothing_window = background::kBackSmoothing9;
                            break;
                        case 11:
                            smoothing_window = background::kBackSmoothing11;
                            break;
                        case 13:
                            smoothing_window = background::kBackSmoothing13;
                            break;
                        case 15:
                            smoothing_window = background::kBackSmoothing15;
                            break;
                    }
                }
                catch (std::logic_error &e)
                { }
                break;
            case 'c':
                compton_estimation = true;
                break;
            case 'v':
                verbose = true;
                break;
            case 'x':
                try
                {
                    min_x_selection = std::stof(optarg);
                }
                catch (std::logic_error &e)
                { }
                break;
            case 'X':
                try
                {
                    max_x_selection = std::stof(optarg);
                }
                catch (std::logic_error &e)
                { }
                break;
            default:
                std::cout << "Unknown command: " << c << std::endl;
                break;
        }
    }

    if (verbose)
    {
        std::cout << "number_of_iterations:" << number_of_iterations << std::endl;
        std::cout << "window_direction:" << window_direction << std::endl;
        std::cout << "filter_order:" << filter_order << std::endl;
        std::cout << "do_smoothing:" << do_smoothing << std::endl;
        std::cout << "smoothing_window:" << smoothing_window << std::endl;
        std::cout << "compton_estimation:" << compton_estimation << std::endl;
    }

    for (int i = optind; i < argc; i++)
    {
        std::string file_name(argv[i]);

        if (verbose)
        {
            std::cout << "Opening file: " << file_name << std::endl;
        }

        std::ifstream in_file(file_name);

        std::vector<double> x_values;
        std::vector<double> y_values;

        while (in_file.good())
        {
            double x;
            double y;

            in_file >> x >> y;

            bool condition = in_file.good();
            condition = condition && (x >= min_x_selection);
            condition = condition && ((max_x_selection > 0 && x <= max_x_selection) || (max_x_selection < 0));

            if (condition)
            {
                x_values.push_back(x);
                y_values.push_back(y);
            }
        }

        unsigned int bins = x_values.size();
        double min_x = x_values.front();
        double max_x = x_values.back();
        double bin_width = (max_x - min_x) / (bins - 1);

        if (verbose)
        {
            std::cout << "bins: " << bins << std::endl;
            std::cout << "min x: " << min_x << std::endl;
            std::cout << "max x: " << max_x << std::endl;
            std::cout << "bin width: " << bin_width << std::endl;
        }

        //TSpectrum t_spectrum;

        //t_spectrum.Background(y_background.data(),
        std::vector<double> y_background = background::background(y_values,
                                                                  number_of_iterations,
                                                                  window_direction,
                                                                  filter_order,
                                                                  do_smoothing,
                                                                  smoothing_window,
                                                                  compton_estimation);

        std::string output_file_name;

        std::size_t found = file_name.find_last_of('.');
        if (found != std::string::npos)
        {
            output_file_name = file_name.substr(0, found);
            output_file_name += "_background";
            output_file_name += file_name.substr(found, std::string::npos);
        }
        else
        {
            output_file_name = file_name;
            output_file_name += "_background";
        }

        if (verbose)
        {
            std::cout << "Writing result to: " << output_file_name << std::endl;
        }

        std::ofstream out_file(output_file_name);
        if (!out_file.good())
        {
            std::cerr << "ERROR: Unable to write to: " << output_file_name << std::endl;
        }
        else
        {
            out_file << "# Data and background from: " << file_name << std::endl;
            out_file << "# x\ty\tsigma_y\tb\tsigma_b\ty - b" << std::endl;

            for (unsigned int i = 0; i < x_values.size(); i++)
            {
                out_file << x_values[i];
                out_file << '\t';
                out_file << y_values[i];
                out_file << '\t';
                out_file << sqrt(y_values[i]);
                out_file << '\t';
                out_file << y_background[i];
                out_file << '\t';
                out_file << sqrt(y_background[i]);
                out_file << '\t';
                out_file << y_values[i] - y_background[i];
                out_file << std::endl;
            }
        }
    }

    return EXIT_SUCCESS;
}
