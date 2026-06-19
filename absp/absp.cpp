/*
 * (C) Copyright 2016,2021,2026 European Union, Cristiano Lino Fontana
 *
 * This file is part of ABCD.
 *
 * ABCD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ABCD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ABCD.  If not, see <http://www.gnu.org/licenses/>.
 */

// For getopt()
#include <unistd.h>
#include <csignal>
// For memcpy()
#include <cstring>
// For nanosleep()
#include <ctime>

#include <map>
#include <iostream>
#include <chrono>
// For std::this_thread::sleep_for
#include <thread>

#include <lua.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

extern "C" {
#include "defaults.h"
#include "utilities_functions.h"
}

#include "ADQ_utilities.hpp"
#include "LuaManager.hpp"

#include "states.hpp"

bool terminate_flag = false;

std::shared_ptr<spdlog::logger> absp_logger_console = nullptr;
std::shared_ptr<spdlog::logger> absp_logger_error = nullptr;

//! Handles standard signals.
/*! SIGTERM (from kill): terminates kindly forcing the status to the closing branch of the state machine.
    SIGINT (from ctrl-c): same behaviour as SIGTERM
    SIGHUP (from shell processes): same behaviour as SIGTERM
 */
void signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM || signum == SIGHUP)
    {
        terminate_flag = true;
    }
}

void print_usage(const std::string &name = std::string("absp")) {
    std::cout << "Usage: " << name << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Data acquisition software that reads data from teledyne digitizers." << std::endl;
    std::cout << std::endl;
    std::cout << "Optional arguments:" << std::endl;
    std::cout << "\t-h: Display this message" << std::endl;
    std::cout << "\t-I: Digitizers identification only" << std::endl;
    std::cout << "\t-S <address>: Status socket address, default: ";
    std::cout << defaults_abcd_status_address << std::endl;
    std::cout << "\t-D <address>: Data output socket address, default: ";
    std::cout << defaults_abcd_data_output_address << std::endl;
    std::cout << "\t-C <address>: Commands socket address, default: ";
    std::cout << defaults_abcd_commands_address << std::endl;
    std::cout << "\t-f <file_name>: Digitizer configuration file, default: ";
    std::cout << defaults_abcd_config_filename << std::endl;
    std::cout << "\t-T <period>: Set base period in milliseconds, default: ";
    std::cout << defaults_abcd_base_period << std::endl;
    std::cout << "\t-v: Set verbose execution, repeating the option increases the verbosity level" << std::endl;
    std::cout << "\t-l <log_filename>: Log to file instead of to the console" << std::endl;

    return;
}

int main(int argc, char *argv[])
{
    // Register the handler for SIGTERM (from kill), SIGINT (from ctrl-c)
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);

    std::string status_address = defaults_abcd_status_address;
    std::string data_output_address = defaults_abcd_data_output_address;
    std::string commands_address = defaults_abcd_commands_address;
    std::string config_filename = defaults_abcd_config_filename;
    std::string log_filename;
    unsigned int base_period = defaults_abcd_base_period;
    unsigned int verbosity = 0;

    bool identification_only = false;

    int c = 0;
    while ((c = getopt(argc, argv, "hIS:D:C:f:T:vl:")) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'I':
                identification_only = true;
                break;
            case 'S':
                status_address = optarg;
                break;
            case 'D':
                data_output_address = optarg;
                break;
            case 'C':
                commands_address = optarg;
                break;
            case 'f':
                config_filename = optarg;
                break;
            case 'T':
                try
                {
                    base_period = std::stoul(optarg);
                } catch (std::logic_error& e) {
                }
                break;
            case 'v':
                verbosity += 1;
                break;
            case 'l':
                log_filename = optarg;
                break;
            default:
                std::cerr << "Unknown command: " << static_cast<char>(c) << std::endl;
                break;
        }
    }

    status global_status;

    global_status.config = NULL;
    global_status.config_filename = config_filename;
    global_status.log_filename = log_filename;
    global_status.status_address = status_address;
    global_status.data_output_address = data_output_address;
    global_status.commands_address = commands_address;
    global_status.identification_only = identification_only;
    global_status.adq_cu_ptr = NULL;


    try
    {
        if (log_filename.length() == 0)
        {
            absp_logger_console = spdlog::stdout_color_mt("logger");
            absp_logger_error = spdlog::stderr_color_mt("error");
        }
        else
        {
            absp_logger_console = spdlog::basic_logger_mt("logger", log_filename);
            absp_logger_error = absp_logger_console;
        }
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        std::cerr << "ERROR: Unable to initiate logger to file: " + log_filename << std::endl;

        return EXIT_FAILURE;
    }

    if (verbosity == 0) {
        absp_logger_console->set_level(spdlog::level::off);
    } else if (verbosity == 1) {
        absp_logger_console->set_level(spdlog::level::info);
    } else if (verbosity == 2) {
        absp_logger_console->set_level(spdlog::level::debug);
    } else if (verbosity == 3) {
        absp_logger_console->set_level(spdlog::level::trace);
    }

    // Overrule the verbosity for identification mode, otherwise nothing is shown
    if (identification_only) {
        absp_logger_console->set_level(spdlog::level::info);
    }

    absp_logger_console->info("Status socket address: {}", status_address);
    absp_logger_console->info("Data output socket address: {}", data_output_address);
    absp_logger_console->info("Commands socket address: {}", commands_address);
    absp_logger_console->info("Configuration file: {}", config_filename);
    absp_logger_console->info("Verbosity: {}", verbosity);
    absp_logger_console->info("Log file: {}", log_filename);
    absp_logger_console->info("Base period: {}", base_period);

    if (identification_only) {
        absp_logger_console->warn("Identification only, the program will quit afterwards");
    }

    state current_state = states::start;

    // Flag for interrupting execution
    bool stop_execution = false;

    while (!stop_execution)
    {
        absp_logger_console->info("Current state ({0}): {1}", current_state.ID, current_state.description);

        if (terminate_flag)
        {
            current_state = states::clear_memory;
            // If we do not clear the flag, we would enter in an infinite loop
            terminate_flag = false;
        }

        if (current_state == states::stop)
            stop_execution = true;

        // Storing this information here because they will change during the
        // state action, but for the post script they need to remain.
        const unsigned int current_state_ID = current_state.ID;
        const std::string current_state_description = current_state.description;

        try {
            const std::pair<unsigned int, unsigned int> script_key_pre(current_state_ID, SCRIPT_WHEN_PRE);
            const std::string script_source_pre = global_status.user_scripts.at(script_key_pre);

            absp_logger_console->info("Running pre script");

            global_status.lua_manager.run_script(current_state_ID,
                                                 current_state_description,
                                                 "pre",
                                                 script_source_pre);

        } catch (...) {}

        current_state = current_state.act(global_status);

        try {
            const std::pair<unsigned int, unsigned int> script_key_post(current_state_ID, SCRIPT_WHEN_POST);
            const std::string script_source_post = global_status.user_scripts.at(script_key_post);

            absp_logger_console->info("Running post script");

            global_status.lua_manager.run_script(current_state_ID,
                                                 current_state_description,
                                                 "post",
                                                 script_source_post);

        } catch (...) {}

        std::this_thread::sleep_for(std::chrono::milliseconds(base_period));
    }

    return 0;
}
