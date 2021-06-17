/*
 * (C) Copyright 2016 Cristiano Lino Fontana
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

// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <zmq.h>

#include <jansson.h>

extern "C" {
#include "defaults.h"
#include "utilities_functions.h"
#include "socket_functions.h"
#include "jansson_socket_functions.h"
}

#include "typedefs.hpp"
#include "events.hpp"
#include "states.hpp"
#include "actions.hpp"

#define BUFFER_SIZE 32

/******************************************************************************/
/* Generic actions                                                            */
/******************************************************************************/

void actions::generic::publish_events(status &global_status)
{
    const size_t waveforms_buffer_size = global_status.waveforms_buffer.size();

    if (waveforms_buffer_size > 0)
    {
        size_t total_size = 0;

        for (auto &event: global_status.waveforms_buffer)
        {
            total_size += event.size();
        }

        std::vector<uint8_t> output_buffer(total_size);

        size_t portion = 0;

        for (auto &event: global_status.waveforms_buffer)
        {
            const size_t event_size = event.size();
            const std::vector<uint8_t> event_buffer = event.serialize();

            memcpy(output_buffer.data() + portion,
                   reinterpret_cast<const void*>(event_buffer.data()),
                   event_size);

            portion += event_size;
        }

        std::string topic = defaults_abcd_data_waveforms_topic;
        topic += "_v0_s";
        topic += std::to_string(total_size);

        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Sending waveforms buffer; ";
            std::cout << "Topic: " << topic << "; ";
            std::cout << "waveforms: " << waveforms_buffer_size << "; ";
            std::cout << "buffer size: " << total_size << "; ";
            std::cout << std::endl;
        }

        const bool result = send_byte_message(global_status.data_socket,
                                              topic.c_str(),
                                              output_buffer.data(),
                                              total_size,
                                              1);

        if (result == EXIT_FAILURE)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR: ZeroMQ Error publishing events";
            std::cout << std::endl;
        }

        // Cleanup vector
        global_status.waveforms_buffer.clear();
        // Initialize vector size to its max size plus a 10%
        global_status.waveforms_buffer.reserve(global_status.events_buffer_max_size
                                               + global_status.events_buffer_max_size / 10);
    }
}

void actions::generic::publish_message(status &global_status,
                                       std::string topic,
                                       json_t *status_message)
{
    std::chrono::time_point<std::chrono::system_clock> last_publication = std::chrono::system_clock::now();
    global_status.last_publication = last_publication;

    void *status_socket = global_status.status_socket;
    const unsigned long int status_msg_ID = global_status.status_msg_ID;

    char time_buffer[BUFFER_SIZE];
    time_string(time_buffer, BUFFER_SIZE, NULL);

    json_object_set_new(status_message, "module", json_string("abcd"));
    json_object_set_new(status_message, "timestamp", json_string(time_buffer));
    json_object_set_new(status_message, "msg_ID", json_integer(status_msg_ID));

    char *output_buffer = json_dumps(status_message, JSON_COMPACT);

    if (!output_buffer)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Unable to allocate status output buffer; ";
        std::cout << std::endl;
    }
    else
    {
        size_t total_size = strlen(output_buffer);

        topic += "_v0_s";
        topic += std::to_string(total_size);

        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Sending status message; ";
            std::cout << "Topic: " << topic << "; ";
            std::cout << "buffer size: " << total_size << "; ";
            std::cout << "message: " << output_buffer << "; ";
            std::cout << std::endl;
        }

        send_byte_message(status_socket, topic.c_str(), output_buffer, total_size, 1);

        free(output_buffer);
    }

    global_status.status_msg_ID += 1;
}

void actions::generic::stop_acquisition(status &global_status)
{
    const unsigned int verbosity = global_status.verbosity;
    PICO_STATUS HowIsPico;

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "#### Stopping acquisition!!! ";
        std::cout << std::endl;
    }

    // Stop acquisition
    HowIsPico = ps5000aStop(global_status.ps5000_handle);
    if (HowIsPico != PICO_OK)
    {
				char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Not able to stop the acquisition!";
        std::cout << std::endl;		
		}

    std::chrono::time_point<std::chrono::system_clock> stop_time = std::chrono::system_clock::now();
    auto delta_time = std::chrono::duration_cast<std::chrono::duration<long int>>(stop_time - global_status.start_time);

    global_status.stop_time = stop_time;

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Run time: ";
        std::cout << delta_time.count();
        std::cout << std::endl;
    }
}

void actions::generic::clear_memory(status &global_status)
{
    global_status.counts.clear();
    global_status.partial_counts.clear();
    global_status.waveforms_buffer.clear();
    global_status.baseline_check_values.clear();
    global_status.trigger_reconfiguration_should_I.clear();
    global_status.trigger_values.clear();
    
    uint32_t capture;
    int16_t channel;
  
		for (auto &channel1: global_status.enabled_channels) 
    {
				for (capture = 0; capture < global_status.Ncaptures; capture++) 
				{
						free(global_status.rapidBuffers[channel1][capture]);
				}
    }
    
    for (channel = 0; channel < global_status.TotNchannel; channel ++) 
    {
				free(global_status.rapidBuffers[channel]);
		}
    
		free(global_status.rapidBuffers);
		free(global_status.overflow);
    
}

void actions::generic::destroy_digitizer(status &global_status)
{
		PICO_STATUS HowIsPico;
    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Destroying digitizer ";
        std::cout << std::endl;
    }

    HowIsPico = ps5000aCloseUnit(global_status.ps5000_handle);
    if (HowIsPico != PICO_OK)
    {
				char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Not able to close the unit!";
        std::cout << std::endl;		
		}
    
    global_status.ps5000_handle = 0;
    
}

bool actions::generic::create_digitizer(status &global_status)
{
    PICO_STATUS HowIsPico;
		HowIsPico = ps5000aOpenUnit(&(global_status.ps5000_handle), NULL, global_status.ps5000_resolution);
		
		if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Opening device number: " << global_status.ps5000_handle << "; ";
        std::cout << std::endl;
    }
		
		if (global_status.ps5000_handle <= 0) 
		{
				char time_buffer[BUFFER_SIZE];
				time_string(time_buffer, BUFFER_SIZE, NULL);
				std::cout << '[' << time_buffer << "] ";
				if (!global_status.ps5000_handle) std::cout << "ERROR: Unable to find the device";
				else std::cout << "ERROR: Unable to open device";
				std::cout << std::endl;
				   
				return false;
		}
			 
		if (HowIsPico == PICO_OK) return true;
		else if (HowIsPico == PICO_POWER_SUPPLY_NOT_CONNECTED)
		{
				char time_buffer[BUFFER_SIZE];
				time_string(time_buffer, BUFFER_SIZE, NULL);
				std::cout << '[' << time_buffer << "] ";
				std::cout << "ERROR: The DC power supply is not connected";
				std::cout << std::endl;
				   
				return false;
		}
		else if (HowIsPico == PICO_INVALID_DEVICE_RESOLUTION) 
		{
				char time_buffer[BUFFER_SIZE];
				time_string(time_buffer, BUFFER_SIZE, NULL);
				std::cout << '[' << time_buffer << "] ";
				std::cout << "ERROR: The resolution is out of range";
				std::cout << std::endl;
				   
				return false;
		}
		else if (HowIsPico == PICO_NOT_FOUND) 
		{
				char time_buffer[BUFFER_SIZE];
				time_string(time_buffer, BUFFER_SIZE, NULL);
				std::cout << '[' << time_buffer << "] ";
				std::cout << "ERROR: No PicoScope could be found";
				std::cout << std::endl;
				   
				return false;
		}
		else 
		{
				char time_buffer[BUFFER_SIZE];
				time_string(time_buffer, BUFFER_SIZE, NULL);
				std::cout << '[' << time_buffer << "] ";
				std::cout << "ERROR: Something wrong has happened";
				std::cout << std::endl;
				   
				return false;
		}
		
    return true;
}

bool actions::generic::configure_digitizer(status &global_status)
{
    unsigned int verbosity = global_status.verbosity;

    global_status.enabled_channels.clear();
		global_status.trigger_values.clear();
    global_status.trigger_values.resize(global_status.TotNchannel, 0);

    json_t *config = global_status.config;
		
		PICO_STATUS HowIsPico;
		std::vector<PS5000A_TRIGGER_CHANNEL_PROPERTIES_V2> triggerProperties;
		std::vector<PS5000A_DIRECTION> triggerDirections;
		std::vector<PS5000A_CONDITION> triggerANDConditions;
		PS5000A_CONDITION triggerORConditionA;
		PS5000A_CONDITION triggerORConditionB;

		HowIsPico = ps5000aPingUnit(global_status.ps5000_handle);
    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";

        if (HowIsPico == PICO_OK)
        {
            std::cout << "Picoscope status: Ok; ";
        }
        else if (HowIsPico == PICO_BUSY)
        {
            std::cout << "Picoscope status: Busy; ";
        }
        else if (HowIsPico == PICO_NOT_RESPONDING)
        {
            std::cout << "Picoscope status: Not responding; ";
        }
        else
        {
            std::cout << "Picoscope status: " << HowIsPico << "; ";
        }
        std::cout << std::endl;
    }
        
    if(HowIsPico != PICO_OK)
    {
				char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Pico is not ok right now!";
        std::cout << std::endl;
    
        return false;
    }

    // Set the resolution
    int16_t pico_resolution = json_number_value(json_object_get(config, "resolution"));
    
    if (pico_resolution < 8 || pico_resolution > 16)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Resolution is out of bounds! ";
        std::cout << "Range: [8, 16] ";
        std::cout << "Got: " << pico_resolution << "; ";
        std::cout << std::endl;

        // Using the default value
        HowIsPico = ps5000aSetDeviceResolution(global_status.ps5000_handle, global_status.ps5000_resolution);
    }
    else if (pico_resolution == 9 || pico_resolution == 10 || pico_resolution == 11 || pico_resolution == 13)
    {
				char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Resolution value not considered! ";
        std::cout << "Value: [8, 10, 12, 14, 15, 16] ";
        std::cout << "Got: " << pico_resolution << "; ";
        std::cout << std::endl;

        // Using the default value
        HowIsPico = ps5000aSetDeviceResolution(global_status.ps5000_handle, global_status.ps5000_resolution);
		}
    else
    {
        if (pico_resolution == 8 ) global_status.ps5000_resolution = PS5000A_DR_8BIT;
        else if (pico_resolution == 12 ) global_status.ps5000_resolution = PS5000A_DR_12BIT;
        else if (pico_resolution == 14 ) global_status.ps5000_resolution = PS5000A_DR_14BIT;
        else if (pico_resolution == 15 ) global_status.ps5000_resolution = PS5000A_DR_15BIT;
        else if (pico_resolution == 16 ) global_status.ps5000_resolution = PS5000A_DR_16BIT;
        
        HowIsPico = ps5000aSetDeviceResolution(global_status.ps5000_handle, global_status.ps5000_resolution);
    }

		if (HowIsPico != PICO_OK)
		{
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Error while setting the resolution -> " << HowIsPico << "; ";
        std::cout << std::endl;

        return false;
    }
    // Reading the set resolution
    PS5000A_DEVICE_RESOLUTION true_pico_resolution;
		HowIsPico = ps5000aGetDeviceResolution(global_status.ps5000_handle, &true_pico_resolution);

    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "True resolution: " << true_pico_resolution;
        if ( true_pico_resolution == PS5000A_DR_8BIT ) std::cout << " -> 8 bits; ";
        else if ( true_pico_resolution == PS5000A_DR_12BIT ) std::cout << " -> 12 bits; ";
        else if ( true_pico_resolution == PS5000A_DR_14BIT ) std::cout << " -> 14 bits; ";
        else if ( true_pico_resolution == PS5000A_DR_15BIT ) std::cout << " -> 15 bits; ";
        else if ( true_pico_resolution == PS5000A_DR_16BIT ) std::cout << " -> 16 bits; ";
        std::cout << std::endl;
    }
		
		// Starting the single channels configuration
    json_t *json_channels = json_object_get(config, "channels");

    if (json_channels != NULL && json_is_array(json_channels))
    {
				size_t index;
				json_t *value;
			
        json_array_foreach(json_channels, index, value) 
        {
						//global_status.TotNchannel ++;
            json_t *json_id = json_object_get(value, "id");

            if (json_id != NULL && json_is_integer(json_id)) {
                
                const int id = json_integer_value(json_id);
                PS5000A_CHANNEL channel_id;
                if(id==0) channel_id = PS5000A_CHANNEL_A;
                else if(id==1) channel_id = PS5000A_CHANNEL_B;
                else
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Channel not well defined! ";
                    std::cout << "Channel: [0 = CHANNEL_A, 1 = CHANNEL_B] ";
                    std::cout << "Got: " << id << "; ";
                    std::cout << std::endl;

                    return false;
                }

                if (verbosity > 0)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Found channel: ";
                    if(id==0) std::cout << "A; ";
                    else if(id==1) std::cout << "B; ";
                    std::cout << std::endl;
                }

                const bool enabled = json_is_true(json_object_get(value, "enable"));
                int16_t enabledInt;
                if (enabled) enabledInt = 1;
                else if (!enabled) enabledInt = 0;

                if (verbosity > 0 && enabled)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Channel is enabled (" << enabledInt << "); ";
                    std::cout << std::endl;
                }
                else if (verbosity > 0 && !enabled)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Channel is disabled (" << enabledInt << "); ";
                    std::cout << std::endl;
                }

                if (enabled)
                {
                    global_status.enabled_channels.push_back(id);
                    global_status.NchannelsEnabled ++;
                }
								
                std::string range_str = json_string_value(json_object_get(value, "range"));
                PS5000A_RANGE range;
                int16_t range_mv;
                if(range_str == "10mV") { range = PS5000A_10MV; range_mv = 10; }
								else if(range_str == "20mV") { range = PS5000A_20MV; range_mv = 20; }
								else if(range_str == "50mV") { range = PS5000A_50MV; range_mv = 50; }
								else if(range_str == "100mV") { range = PS5000A_100MV; range_mv = 100; }
								else if(range_str == "200mV") { range = PS5000A_200MV; range_mv = 200; }
								else if(range_str == "500mV") { range = PS5000A_500MV; range_mv = 500; }
								else if(range_str == "1V") { range = PS5000A_1V; range_mv = 1000; }
								else if(range_str == "2V") { range = PS5000A_2V; range_mv = 2000; }
								else if(range_str == "5V") { range = PS5000A_5V; range_mv = 5000; }
								else if(range_str == "10V") { range = PS5000A_10V; range_mv = 10000; }
								else if(range_str == "20V") { range = PS5000A_20V; range_mv = 20000; }
								else
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Range is not well defined! ";
                    std::cout << "Range: [10mV, 20mV, 50mV, 100mV, 200mV, 500mV, 1V, 2V, 5V, 10V, 20V] ";
                    std::cout << "Got: " << range_str << "; ";
                    std::cout << std::endl;

                    // Setting a default range value
                    range = PS5000A_500MV;
                }

                if (verbosity > 0)
                {
										char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Using range: " << range << " -> " << range_str << "; ";
                    std::cout << std::endl;
                }
								
								// Set the coupling
								std::string coupling_str = json_string_value(json_object_get(value, "coupling"));
                PS5000A_COUPLING coupling;
                if(coupling_str == "AC") coupling = PS5000A_AC;
								else if(coupling_str == "DC") coupling = PS5000A_DC;
								else
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Coupling is not well defined! ";
                    std::cout << "Couplings: [AC, DC] ";
                    std::cout << "Got: " << coupling_str << "; ";
                    std::cout << std::endl;

                    // Setting a default coupling
                    coupling = PS5000A_DC;
                }

                if (verbosity > 0)
                {
										char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "Using coupling: " << coupling << " -> " << coupling_str << "; ";
                    std::cout << std::endl;
                }
								
                // Reading the channel offset info
								float offset_min;
								float offset_max;
								HowIsPico = ps5000aGetAnalogueOffset(global_status.ps5000_handle, range, coupling, &offset_max, &offset_min);
								if (HowIsPico != PICO_OK)
								{
										char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Not able to get possible analogue offset! ";
                    std::cout << std::endl;

										return false;
								}
                
                // Get offset value
                float offset = json_number_value(json_object_get(value, "offset"));

                if (offset < offset_min || offset_max < offset)
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Offset is out of bounds! ";
                    std::cout << "Range: [" << offset_min << ", " << offset_max << "] ";
                    std::cout << "Got: " << offset << "; ";
                    std::cout << std::endl;

                    offset = (offset_max + offset_min) / 2;
                }
                if (verbosity > 0)
								{
										char time_buffer[BUFFER_SIZE];
										time_string(time_buffer, BUFFER_SIZE, NULL);
        						std::cout << '[' << time_buffer << "] ";
        						std::cout << "offset min: " << offset_min << "; ";
        						std::cout << "offset max: " << offset_max << "; ";
        						std::cout << "offset set: " << offset << "; ";
        						std::cout << std::endl;
    						}
                                
                // Setting the channel
                HowIsPico = ps5000aSetChannel(global_status.ps5000_handle, channel_id, enabledInt, coupling, range, offset);
                if (HowIsPico != PICO_OK)
								{
										char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Not able to set channel " << id << "; ";
                    std::cout << std::endl;

										return false;
								}
								
								// Filling the vector with trigger properties
                int16_t thr_mv = json_number_value(json_object_get(value, "trigger_level_mv"));
                int16_t maxADCValue;
                HowIsPico = ps5000aMaximumValue(global_status.ps5000_handle, &maxADCValue);
                int16_t thr_ADC = (thr_mv * maxADCValue) / range_mv;

                if (HowIsPico != PICO_OK)
								{
										char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Not able to get the ADC max value! ";
                    std::cout << std::endl;

										return false;
								}
								
								if(thr_ADC <= -maxADCValue || thr_ADC >= maxADCValue)
								{
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Trigger level is out of bounds! ";
                    std::cout << "Range: [" << -maxADCValue << ", " << maxADCValue << "] ";
                    std::cout << "Got: " << thr_ADC << " (" << thr_mv << " mV); ";
                    std::cout << std::endl;

                    thr_ADC = ( maxADCValue / 2 );
                }
                
								global_status.trigger_values[id] = thr_ADC;

                if(enabled) triggerProperties.push_back({thr_ADC, 0, 0, 0, channel_id});
                if (verbosity > 0 && enabled)
								{
										char time_buffer[BUFFER_SIZE];
										time_string(time_buffer, BUFFER_SIZE, NULL);
        						std::cout << '[' << time_buffer << "] ";
        						std::cout << "Trigger threshold got [mV]: " << thr_mv << "; ";
        						std::cout << "Trigger threshold calibrated to adc: " << triggerProperties.back().thresholdUpper << "; ";
        						std::cout << std::endl;
    						}
                
                // Filling the vector with trigger conditions
                std::string trigger_cond_str = json_string_value(json_object_get(value, "trigger_condition"));
                
                if( trigger_cond_str == "OR" && enabled) // If OR, channel conditions to be set indipendently for each channel
                {
										PS5000A_CONDITIONS_INFO trigger_cond_info;
										if(id==0) 
										{
												triggerORConditionA.source = channel_id;
												triggerORConditionA.condition = PS5000A_CONDITION_TRUE;
												trigger_cond_info = (PS5000A_CONDITIONS_INFO) (PS5000A_CLEAR | PS5000A_ADD);
												HowIsPico = ps5000aSetTriggerChannelConditionsV2( global_status.ps5000_handle, &triggerORConditionA, 1, trigger_cond_info);
												if (HowIsPico != PICO_OK)
												{
														char time_buffer[BUFFER_SIZE];
														time_string(time_buffer, BUFFER_SIZE, NULL);
														std::cout << '[' << time_buffer << "] ";
														std::cout << "ERROR: Not able to set the " << id << " channel trigger OR conditions! ";
														std::cout << std::endl;

														return false;
												}
												if (verbosity > 0)
												{
														char time_buffer[BUFFER_SIZE];
														time_string(time_buffer, BUFFER_SIZE, NULL);
        										std::cout << '[' << time_buffer << "] ";
        										std::cout << "Trigger conditions set: " << trigger_cond_str << "(" << triggerORConditionA.condition << ") for channel " << id << "; ";
        										std::cout << std::endl;
												}
										}
										else if (id==1 && global_status.NchannelsEnabled > 1) 
										{
												triggerORConditionB.source = channel_id;
												triggerORConditionB.condition = PS5000A_CONDITION_TRUE;
												trigger_cond_info = PS5000A_ADD;
												
												HowIsPico = ps5000aSetTriggerChannelConditionsV2( global_status.ps5000_handle, &triggerORConditionB, 1, trigger_cond_info);
												if (HowIsPico != PICO_OK)
												{
														char time_buffer[BUFFER_SIZE];
														time_string(time_buffer, BUFFER_SIZE, NULL);
														std::cout << '[' << time_buffer << "] ";
														std::cout << "ERROR: Not able to set the " << id << " channel trigger OR conditions! ";
														std::cout << std::endl;

														return false;
												}
												if (verbosity > 0)
												{
														char time_buffer[BUFFER_SIZE];
														time_string(time_buffer, BUFFER_SIZE, NULL);
        										std::cout << '[' << time_buffer << "] ";
        										std::cout << "Trigger conditions set: " << trigger_cond_str << "(" << triggerORConditionB.condition << ") for channel " << id << "; ";
        										std::cout << std::endl;
												}
										}
								}
								else if ( trigger_cond_str == "AND" && enabled) // If AND, channel conditions to be set once for every channel
								{
										//PS5000A_CONDITION temp_cond;
										//temp_cond.source = channel_id;
										//temp_cond.condition = PS5000A_CONDITION_TRUE;
										triggerANDConditions.push_back({channel_id, PS5000A_CONDITION_TRUE});
										if (verbosity > 0)
										{
												char time_buffer[BUFFER_SIZE];
												time_string(time_buffer, BUFFER_SIZE, NULL);
        								std::cout << '[' << time_buffer << "] ";
        								std::cout << "Trigger conditions set: " << trigger_cond_str << "(" << triggerANDConditions.back().condition << ") for channel " << id << "; ";
        								std::cout << std::endl;
										}
								}
								else if ( (trigger_cond_str != "AND" || trigger_cond_str != "OR") && enabled)
								{
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Trigger conditions not well defined! ";
                    std::cout << "Conditions: AND / OR ";
                    std::cout << "Got: " << trigger_cond_str << "; ";
                    std::cout << std::endl;

                    return false;
                }
                                
                // Filling the vector with trigger directions
                std::string trigger_dir_str = json_string_value(json_object_get(value, "trigger_direction"));
                PS5000A_THRESHOLD_DIRECTION trigger_dir;
                if( trigger_dir_str == "RISING" ) trigger_dir = PS5000A_RISING;
                else if ( trigger_dir_str == "FALLING" ) trigger_dir = PS5000A_FALLING;
                else
                {
                    char time_buffer[BUFFER_SIZE];
                    time_string(time_buffer, BUFFER_SIZE, NULL);
                    std::cout << '[' << time_buffer << "] ";
                    std::cout << "ERROR: Trigger directions not well defined! ";
                    std::cout << "Directions: RISING / FALLING ";
                    std::cout << "Got: " << trigger_dir_str << "; ";
                    std::cout << std::endl;

                    trigger_dir = PS5000A_FALLING;
                }

								global_status.ps5000_1ch_trigger_dir = trigger_dir;
                
                if (verbosity > 0 && enabled)
								{
										char time_buffer[BUFFER_SIZE];
										time_string(time_buffer, BUFFER_SIZE, NULL);
        						std::cout << '[' << time_buffer << "] ";
        						std::cout << "Trigger direction got: " << trigger_dir << " -> " << trigger_dir_str << "; ";
        						std::cout << std::endl;
    						}
                
                if(enabled) triggerDirections.push_back( {channel_id, trigger_dir, PS5000A_LEVEL} );
                                
            }
        }
    }
    
    if (verbosity > 0)
		{
				char time_buffer[BUFFER_SIZE];
				time_string(time_buffer, BUFFER_SIZE, NULL);
    		std::cout << '[' << time_buffer << "] ";
    		std::cout << "Enabled and set channels: " << global_status.NchannelsEnabled << "; " << std::endl;
    		std::cout << std::endl;
    }
		
		// Conclude to set the trigger
		if( global_status.NchannelsEnabled == 1 ) // Set the conditions indipendently from OR or AND
		{
				PS5000A_CHANNEL en_ch = PS5000A_CHANNEL_A;
				if(global_status.enabled_channels.back() == 0) en_ch = PS5000A_CHANNEL_A;
				else if(global_status.enabled_channels.back() == 1) en_ch = PS5000A_CHANNEL_B;
				HowIsPico = ps5000aSetSimpleTrigger(global_status.ps5000_handle, 
																						1, 
																						en_ch, 
																						triggerProperties.back().thresholdUpper, 
																						triggerDirections.back().direction, 
																						0, 
																						0);
				if (HowIsPico != PICO_OK)
				{
						char time_buffer[BUFFER_SIZE];
						time_string(time_buffer, BUFFER_SIZE, NULL);
						std::cout << '[' << time_buffer << "] ";
						std::cout << "ERROR: Not able to set the channel trigger conditions! ";
						std::cout << std::endl;

						return false;
				}
		}
		else if (global_status.NchannelsEnabled > 1)
		{
				if ( triggerANDConditions.size() > 1 ) // OR conditions already set, AND are missing
				{
						HowIsPico = ps5000aSetTriggerChannelConditionsV2( global_status.ps5000_handle, 
																															triggerANDConditions.data(), 
																															triggerANDConditions.size(),
																															(PS5000A_CONDITIONS_INFO) (PS5000A_CLEAR | PS5000A_ADD) );
						if (HowIsPico != PICO_OK)
						{
								char time_buffer[BUFFER_SIZE];
								time_string(time_buffer, BUFFER_SIZE, NULL);
								std::cout << '[' << time_buffer << "] ";
								std::cout << "ERROR: Not able to set the channel trigger AND conditions! ";
								std::cout << std::endl;

								return false;
						}
				}
				
				int16_t auxOutputEnable = 0; // not used
				HowIsPico = ps5000aSetTriggerChannelPropertiesV2( global_status.ps5000_handle, 
																													triggerProperties.data(), 
																													triggerProperties.size(), 
																													auxOutputEnable);
				if (HowIsPico != PICO_OK)
				{
						char time_buffer[BUFFER_SIZE];
						time_string(time_buffer, BUFFER_SIZE, NULL);
						std::cout << '[' << time_buffer << "] ";
						std::cout << "ERROR: Not able to set channel properties! ";
						std::cout << std::endl;

						return false;
				}
				
				HowIsPico = ps5000aSetTriggerChannelDirectionsV2( global_status.ps5000_handle, 
																													triggerDirections.data(), 
																													triggerDirections.size());
				if (HowIsPico != PICO_OK)
				{
						char time_buffer[BUFFER_SIZE];
						time_string(time_buffer, BUFFER_SIZE, NULL);
						std::cout << '[' << time_buffer << "] ";
						std::cout << "ERROR: Not able to set channels directions! ";
						std::cout << std::endl;

						return false;
				}
		
				HowIsPico = ps5000aSetAutoTriggerMicroSeconds(global_status.ps5000_handle, 0);
				if (HowIsPico != PICO_OK)
				{
						char time_buffer[BUFFER_SIZE];
						time_string(time_buffer, BUFFER_SIZE, NULL);
						std::cout << '[' << time_buffer << "] ";
						std::cout << "ERROR: Not able to set auto-trigger! ";
						std::cout << std::endl;

						return false;
				}
		
				HowIsPico = ps5000aSetTriggerDelay(global_status.ps5000_handle, 0);
				if (HowIsPico != PICO_OK)
				{
						char time_buffer[BUFFER_SIZE];
						time_string(time_buffer, BUFFER_SIZE, NULL);
						std::cout << '[' << time_buffer << "] ";
						std::cout << "ERROR: Not able to set trigger delay! ";
						std::cout << std::endl;

						return false;
				}
		}
		else
		{
				char time_buffer[BUFFER_SIZE];
				time_string(time_buffer, BUFFER_SIZE, NULL);
				std::cout << '[' << time_buffer << "] ";
				std::cout << "ERROR: Strange number of enabled channel ";
				std::cout << std::endl;

				return false;
		}
		
		if (verbosity > 0)
		{
				char time_buffer[BUFFER_SIZE];
				time_string(time_buffer, BUFFER_SIZE, NULL);
    		std::cout << '[' << time_buffer << "] ";
    		std::cout << "End of trigger configuration; ";
    		std::cout << std::endl;
    }
		
		// Get correct timebase
		int32_t MaxNSamples;
		global_status.Nsamples = json_number_value(json_object_get(config, "sampleCount"));
		float timebase = json_number_value(json_object_get(config, "timebase"));
		
		do
		{
				HowIsPico = ps5000aGetTimebase(global_status.ps5000_handle, (uint32_t)timebase, global_status.Nsamples, &global_status.timeIntervalNs, &MaxNSamples, 0);
				if (HowIsPico == PICO_INVALID_TIMEBASE)
				{
						char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR: Invalid timebase set " << timebase << "; ";
            std::cout << std::endl;
						timebase++;
				}
		} while (HowIsPico != PICO_OK);
		
		global_status.timebase = timebase;
		
		if (verbosity > 0)
		{
				char time_buffer[BUFFER_SIZE];
				time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Timebase set: " << global_status.timebase << "; ";
        std::cout << std::endl;
    }
		
		// Check and set memory segments
		global_status.Ncaptures = json_number_value(json_object_get(config, "block_Ncaptures"));
		uint32_t nMaxSegments;
		HowIsPico = ps5000aGetMaxSegments(global_status.ps5000_handle, &nMaxSegments);
		if (HowIsPico != PICO_OK)
		{
				char time_buffer[BUFFER_SIZE];
				time_string(time_buffer, BUFFER_SIZE, NULL);
				std::cout << '[' << time_buffer << "] ";
				std::cout << "ERROR: Not able to get the maximum number of segments! ";
				std::cout << std::endl;

				return false;
		}
		
		if (global_status.Ncaptures > nMaxSegments)
		{
				char time_buffer[BUFFER_SIZE];
				time_string(time_buffer, BUFFER_SIZE, NULL);
				std::cout << '[' << time_buffer << "] ";
				std::cout << "ERROR: Captures set (" << global_status.Ncaptures << ") are too much. Limit: " << nMaxSegments << " ! ";
				std::cout << std::endl;

				global_status.Ncaptures = nMaxSegments;
		}
		
		int32_t nMaxSamples;		
		HowIsPico = ps5000aMemorySegments(global_status.ps5000_handle, global_status.Ncaptures, &nMaxSamples);
		if (HowIsPico != PICO_OK)
		{
				char time_buffer[BUFFER_SIZE];
				time_string(time_buffer, BUFFER_SIZE, NULL);
				std::cout << '[' << time_buffer << "] ";
				std::cout << "ERROR: Not able to get memory segments! ";
				std::cout << std::endl;

				return false;
		}
		
		if (nMaxSamples/global_status.NchannelsEnabled < global_status.Nsamples)
		{
				char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: sampleCount set (" << global_status.Nsamples << ") is exceeding the max possible for each channel: " << nMaxSamples/global_status.NchannelsEnabled <<  "! ";
        std::cout << std::endl;
				
				return false;
     }
     
     // Set number of captures
     HowIsPico = ps5000aSetNoOfCaptures(global_status.ps5000_handle, global_status.Ncaptures);
     if (HowIsPico != PICO_OK)
		 {
				char time_buffer[BUFFER_SIZE];
				time_string(time_buffer, BUFFER_SIZE, NULL);
				std::cout << '[' << time_buffer << "] ";
				std::cout << "ERROR: Not able to set the number of captures! ";
				std::cout << std::endl;

				return false;
		 }        
                
     if (verbosity > 0)
		 {
				char time_buffer[BUFFER_SIZE];
				time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Memory segmented, number of samples for each waveform set to: " << global_status.Nsamples << ", number of captures for each block set to: " << global_status.Ncaptures << "; ";
        std::cout << std::endl;
     }
     
     // Read number of pretrigger samples
     int32_t NpretriggerSamples = json_number_value(json_object_get(config, "pretrigger"));
     if (NpretriggerSamples >= global_status.Nsamples)
		 {
				char time_buffer[BUFFER_SIZE];
				time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Number of pretrigger samples not correct! ";
        std::cout << std::endl;
        
        NpretriggerSamples = global_status.Nsamples / 3;
     }
     
     global_status.NpretriggerSamples = NpretriggerSamples;
     
     if (verbosity > 0)
		 {
				char time_buffer[BUFFER_SIZE];
				time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Number of pretrigger samples set to: " << global_status.NpretriggerSamples << "; ";
        std::cout << std::endl;
     }
     
    return true;
}

bool actions::generic::allocate_memory(status &global_status)
{
    unsigned int verbosity = global_status.verbosity;

    global_status.counts.clear();
    global_status.counts.resize(global_status.TotNchannel, 0);
    global_status.partial_counts.clear();
    global_status.partial_counts.resize(global_status.TotNchannel, 0);
    global_status.baseline_check_values.clear();
    global_status.baseline_check_values.resize(global_status.TotNchannel, 0.);
    global_status.trigger_reconfiguration_should_I.clear();
    global_status.trigger_reconfiguration_should_I.resize(global_status.TotNchannel, false);
   
    if (verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Memory allocation";
        std::cout << std::endl;
    }

    // Reserve the events_buffer in order to have a good starting size of its buffer
    global_status.waveforms_buffer.reserve(global_status.events_buffer_max_size
                                           + global_status.events_buffer_max_size / 10);
    
    uint32_t capture;   
    int16_t channel;                                    
    PICO_STATUS HowIsPico;
    //Allocate memory (FOR ALL CHANNELS)
		global_status.rapidBuffers = (int16_t ***) calloc(global_status.TotNchannel, sizeof(int16_t*));
		global_status.overflow = (int16_t *) calloc(global_status.TotNchannel * global_status.Ncaptures, sizeof(int16_t));
  
		for (channel = 0; channel < global_status.TotNchannel; channel++) 
    {
				global_status.rapidBuffers[channel] = (int16_t **) calloc(global_status.Ncaptures, sizeof(int16_t*));
		}
  
		for (auto &channel2: global_status.enabled_channels) 
    {
				for (capture = 0; capture < global_status.Ncaptures; capture++) 
				{
						global_status.rapidBuffers[channel2][capture] = (int16_t *) calloc(global_status.Nsamples, sizeof(int16_t));
				}
    }
		
		// Set buffer (ONLY FOR ENABLED CHANNELS)
		PS5000A_CHANNEL pschannel;
    for (auto &channel1: global_status.enabled_channels) 
    {
				if(channel1==0) pschannel = PS5000A_CHANNEL_A;
        else if(channel1==1) pschannel = PS5000A_CHANNEL_B;
        // Other scopes could have more than 2 channels..
        
				for (capture = 0; capture < global_status.Ncaptures; capture++) 
				{
						HowIsPico = ps5000aSetDataBuffer(global_status.ps5000_handle, 
																						 (PS5000A_CHANNEL)pschannel, 
																						 global_status.rapidBuffers[channel1][capture], 
																						 global_status.Nsamples, 
																						 capture, 
																						 PS5000A_RATIO_MODE_NONE);
						
						if (HowIsPico != PICO_OK)
						{
								char time_buffer[BUFFER_SIZE];
								time_string(time_buffer, BUFFER_SIZE, NULL);
								std::cout << '[' << time_buffer << "] ";
								std::cout << "ERROR: Not able to set the data buffer of channel " << channel1 << ", capture " << capture << "! ";
								std::cout << std::endl;

								return false;
						}
				}
		}
      
    return true;
}

/******************************************************************************/
/* Specific actions                                                           */
/******************************************************************************/

state actions::start(status&)
{
    return states::CREATE_CONTEXT;
}

state actions::create_context(status &global_status)
{
    // Creates a ØMQ context
    void *context = zmq_ctx_new();
    if (!context)
    {
        // No errors are defined for this function
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on context creation";
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    global_status.context = context;

    std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));

    return states::CREATE_SOCKETS;
}

state actions::create_sockets(status &global_status)
{
    void *context = global_status.context;

    // Creates the status socket
    void *status_socket = zmq_socket(context, ZMQ_PUB);
    if (!status_socket)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on status socket creation: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Creates the data socket
    void *data_socket = zmq_socket(context, ZMQ_PUB);
    if (!data_socket)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on data socket creation: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Creates the commands socket
    void *commands_socket = zmq_socket(context, ZMQ_PULL);
    if (!commands_socket)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on commands socket creation: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    global_status.status_socket = status_socket;
    global_status.data_socket = data_socket;
    global_status.commands_socket = commands_socket;

    std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));

    return states::BIND_SOCKETS;
}

state actions::bind_sockets(status &global_status)
{
    std::string status_address = global_status.status_address;
    std::string data_address = global_status.data_address;
    std::string commands_address = global_status.commands_address;

    // Binds the status socket to its address
    const int s = zmq_bind(global_status.status_socket, status_address.c_str());
    if (s != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on status socket binding: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Binds the data socket to its address
    const int d = zmq_bind(global_status.data_socket, data_address.c_str());
    if (d != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on data socket binding: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    // Binds the socket to its address
    const int c = zmq_bind(global_status.commands_socket, commands_address.c_str());
    if (c != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: ZeroMQ Error on commands socket binding: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;

        return states::COMMUNICATION_ERROR;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));

    return states::READ_CONFIG;
}

state actions::read_config(status &global_status)
{
    const std::string config_file_name = global_status.config_file;

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Reading config file: " << config_file_name << " ";
        std::cout << std::endl;
    }

    json_error_t error;

    json_t *new_config = json_load_file(config_file_name.c_str(), 0, &error);

    if (!new_config)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Parse error while reading config file: ";
        std::cout << error.text;
        std::cout << " (source: ";
        std::cout << error.source;
        std::cout << ", line: ";
        std::cout << error.line;
        std::cout << ", column: ";
        std::cout << error.column;
        std::cout << ", position: ";
        std::cout << error.position;
        std::cout << "); ";
        std::cout << std::endl;

        return states::PARSE_ERROR;
    }
    else
    {
        global_status.config = new_config;

        return states::CREATE_DIGITIZER;
    }
}

state actions::create_digitizer(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string("Digitizer initialization"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    const bool success = actions::generic::create_digitizer(global_status);

    if (success)
    {
        return states::CONFIGURE_DIGITIZER;
    }
    else
    {
        return states::CONFIGURE_ERROR;
    }
}

state actions::recreate_digitizer(status &global_status)
{
    const bool success = actions::generic::create_digitizer(global_status);

    if (success)
    {
        return states::CONFIGURE_DIGITIZER;
    }
    else
    {
        return states::CONFIGURE_ERROR;
    }
}

state actions::configure_digitizer(status &global_status)
{
    const bool success = actions::generic::configure_digitizer(global_status);

    if (success)
    {
        return states::ALLOCATE_MEMORY;
    }
    else
    {
        return states::CONFIGURE_ERROR;
    }
}

state actions::reconfigure_destroy_digitizer(status &global_status)
{
    actions::generic::destroy_digitizer(global_status);

    return states::RECREATE_DIGITIZER;
}

state actions::receive_commands(status &global_status)
{
    void *commands_socket = global_status.commands_socket;

    char *buffer;
    size_t size;
    
    const int result = receive_byte_message(commands_socket, nullptr, (void**)(&buffer), &size, 0, global_status.verbosity);

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "result: " << result << "; ";
        std::cout << "size: " << size << "; ";
        std::cout << std::endl;
    }

    if (size > 0 && result == EXIT_SUCCESS)
    {
        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Message buffer: ";
            std::cout << (char*)buffer;
            std::cout << "; ";
            std::cout << std::endl;
        }

        std::string error_string;

        json_error_t error;
        json_t *json_message = json_loadb(buffer, size, 0, &error);

        free(buffer);

        if (!json_message)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR: ";
            std::cout << error.text;
            std::cout << " (source: ";
            std::cout << error.source;
            std::cout << ", line: ";
            std::cout << error.line;
            std::cout << ", column: ";
            std::cout << error.column;
            std::cout << ", position: ";
            std::cout << error.position;
            std::cout << "); ";
            std::cout << std::endl;
        }
        else
        {
            const size_t command_ID = json_integer_value(json_object_get(json_message, "msg_ID"));
            const std::string command = json_string_value(json_object_get(json_message, "command"));
            json_t *json_arguments = json_object_get(json_message, "arguments");

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Command ID: " << command_ID << "; ";
                std::cout << std::endl;
            }

            if (command == std::string("start"))
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "################################################################### Start!!! ###";
                std::cout << std::endl;

                return states::START_ACQUISITION;
            }
            else if (command == std::string("reconfigure") && json_arguments)
            {
								json_t *new_config = json_object_get(json_arguments, "config");

                // Remember to clean up
                json_decref(global_status.config);

                // This is a borrowed reference, so we shall increment the reference count
                global_status.config = new_config;
                json_incref(global_status.config);

                json_t *json_event_message = json_object();

                json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
                json_object_set_new_nocheck(json_event_message, "event", json_string("Digitizer reconfiguration"));

                actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

                json_decref(json_event_message);
										
                return states::CONFIGURE_DIGITIZER;
            }
            else if (command == std::string("off"))
            {
                return states::CLEAR_MEMORY;
            }
            else if (command == std::string("quit"))
            {
                return states::CLEAR_MEMORY;
            }

            json_decref(json_message);
        }
    }

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    if (now - global_status.last_publication > std::chrono::seconds(defaults_abcd_publish_timeout))
    {
        return states::PUBLISH_STATUS;
    }

    return states::RECEIVE_COMMANDS;
}

state actions::publish_status(status &global_status)
{
    PICO_STATUS HowIsPico;
    HowIsPico = ps5000aPingUnit(global_status.ps5000_handle);

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";

        if (HowIsPico == PICO_OK)
        {
            std::cout << "Picoscope status: Ok; ";
        }
        else if (HowIsPico == PICO_BUSY)
        {
            std::cout << "Picoscope status: Busy; ";
        }
        else if (HowIsPico == PICO_NOT_RESPONDING)
        {
            std::cout << "Picoscope status: Not responding; ";
        }
        else
        {
            std::cout << "Picoscope status: " << HowIsPico << "; ";
        }
        std::cout << std::endl;
    }

    json_t *status_message = json_object();

    json_object_set_new_nocheck(status_message, "config", json_deep_copy(global_status.config));

    json_t *acquisition = json_object();
    json_object_set_new_nocheck(acquisition, "running", json_false());
    json_object_set_new_nocheck(status_message, "acquisition", acquisition);

    json_t *digitizer = json_object();

    if (HowIsPico != PICO_OK)
    {
        json_object_set_new_nocheck(digitizer, "valid_pointer", json_false());
        json_object_set_new_nocheck(digitizer, "active", json_false());
    }
    else
    {
        json_object_set_new_nocheck(digitizer, "valid_pointer", json_true());
        json_object_set_new_nocheck(digitizer, "active", json_true());
    }

    json_object_set_new_nocheck(status_message, "digitizer", digitizer);

    actions::generic::publish_message(global_status, defaults_abcd_status_topic, status_message);

    json_decref(status_message);

    if (HowIsPico == PICO_OK)
    {
        return states::RECEIVE_COMMANDS;
    }
    else
    {
        return states::DIGITIZER_ERROR;
    }
}

state actions::allocate_memory(status &global_status)
{
    const bool success = actions::generic::allocate_memory(global_status);

    if (success)
    {
        return states::PUBLISH_STATUS;
    }
    else
    {
        return states::DIGITIZER_ERROR;
    }
}

state actions::restart_allocate_memory(status &global_status)
{
    const bool success = actions::generic::allocate_memory(global_status);

    if (success)
    {
        return states::START_ACQUISITION;
    }
    else
    {
        return states::RESTART_CONFIGURE_ERROR;
    }
}

state actions::stop(status&)
{
    return states::STOP;
}

state actions::start_acquisition(status &global_status)
{
    PICO_STATUS HowIsPico;
   
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string("Start acquisition"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    HowIsPico = ps5000aPingUnit(global_status.ps5000_handle);
    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";

        if (HowIsPico == PICO_OK)
        {
            std::cout << "Picoscope status: Ok; ";
        }
        else if (HowIsPico == PICO_BUSY)
        {
            std::cout << "Picoscope status: Busy; ";
        }
        else if (HowIsPico == PICO_NOT_RESPONDING)
        {
            std::cout << "Picoscope status: Not responding; ";
        }
        else
        {
            std::cout << "Picoscope status: " << HowIsPico << "; ";
        }
        std::cout << std::endl;
    }
        
    if(HowIsPico != PICO_OK)
    {
				char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Pico is not ok right now!";
        std::cout << std::endl;
    
        return states::ACQUISITION_ERROR;
    }

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Clearing counters; ";
        std::cout << std::endl;
    }

    global_status.counts.clear();
    global_status.counts.resize(global_status.TotNchannel, 0);
    global_status.partial_counts.clear();
    global_status.partial_counts.resize(global_status.TotNchannel, 0);
    global_status.baseline_check_values.clear();
    global_status.baseline_check_values.resize(global_status.TotNchannel, 0.);
    global_status.trigger_reconfiguration_should_I.clear();
    global_status.trigger_reconfiguration_should_I.resize(global_status.TotNchannel, false);
    global_status.BlockNumber = 0;

    // Start acquisition 
    std::chrono::time_point<std::chrono::system_clock> start_time = std::chrono::system_clock::now();
    
    HowIsPico = ps5000aRunBlock( global_status.ps5000_handle, 
																 global_status.NpretriggerSamples, 
																 global_status.Nsamples - global_status.NpretriggerSamples, 
																 global_status.timebase, 
																 NULL, 
																 0, 
																 NULL, 
																 NULL );
		
		if (HowIsPico != PICO_OK)
    {
				char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Not able to start the acquisition!";
        std::cout << std::endl;		
        
        return states::ACQUISITION_ERROR;
		}

    global_status.start_time = start_time;
    global_status.block_start_time = start_time;
    global_status.BlockNumber++;

    return states::ACQUISITION_RECEIVE_COMMANDS;
}

state actions::acquisition_receive_commands(status &global_status)
{
    void *commands_socket = global_status.commands_socket;

    char *buffer;
    size_t size;
    
    const int result = receive_byte_message(commands_socket, nullptr, (void**)(&buffer), &size, 0, global_status.verbosity);

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "result: " << result << "; ";
        std::cout << "size: " << size << "; ";
        std::cout << std::endl;
    }

    if (size > 0 && result == EXIT_SUCCESS)
    {
        if (global_status.verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Message buffer: ";
            std::cout << (char*)buffer;
            std::cout << "; ";
            std::cout << std::endl;
        }

        std::string error_string;

        json_error_t error;
        json_t *json_message = json_loadb(buffer, size, 0, &error);

        free(buffer);

        if (!json_message)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "ERROR: ";
            std::cout << error.text;
            std::cout << " (source: ";
            std::cout << error.source;
            std::cout << ", line: ";
            std::cout << error.line;
            std::cout << ", column: ";
            std::cout << error.column;
            std::cout << ", position: ";
            std::cout << error.position;
            std::cout << "); ";
            std::cout << std::endl;
        }
        else
        {
            const size_t command_ID = json_integer_value(json_object_get(json_message, "msg_ID"));
            const std::string command = json_string_value(json_object_get(json_message, "command"));

            if (global_status.verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Command ID: " << command_ID << "; ";
                std::cout << std::endl;
            }

            if (command == std::string("stop"))
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "#################################################################### Stop!!! ###";
                std::cout << std::endl;

                return states::STOP_PUBLISH_EVENTS;
            }

            json_decref(json_message);
        }
    }

    return states::ADD_TO_BUFFER;
}

state actions::add_to_buffer(status &global_status)
{
    const unsigned int verbosity = global_status.verbosity;

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();

    PICO_STATUS HowIsPico;
    int16_t poll_success;
    uint32_t CompletedCaptures = global_status.Ncaptures;
		HowIsPico = ps5000aIsReady(global_status.ps5000_handle, &poll_success);

    if (HowIsPico != PICO_OK)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Device is not responding!; ";
        std::cout << std::endl;
        
        return states::ACQUISITION_ERROR;
    }

		if ( poll_success != 0 || (now - global_status.last_publication > std::chrono::seconds(defaults_abps5000a_publish_timeout)) )
    {
				const std::chrono::time_point<std::chrono::system_clock> block_end_time = std::chrono::system_clock::now();
        // If I pass the timeout interval, I have to stop the acquisition and see how many captures have been collected
        // If the device complete the acquisition of the block this is not needed      
        if( poll_success == 0 )
        {
						// Before stopping, check if there are captures.. otherwise continue the acquisition  
						HowIsPico = ps5000aGetNoOfCaptures(global_status.ps5000_handle, &CompletedCaptures);
						if (HowIsPico != PICO_OK)
						{
								char time_buffer[BUFFER_SIZE];
								time_string(time_buffer, BUFFER_SIZE, NULL);
								std::cout << '[' << time_buffer << "] ";
								std::cout << "ERROR: Not able to get the number of completed captures!";
								std::cout << std::endl;		
								
								return states::ACQUISITION_ERROR;
						}
					
						if (CompletedCaptures == 0)
						{
								char time_buffer[BUFFER_SIZE];
								time_string(time_buffer, BUFFER_SIZE, NULL);
								std::cout << '[' << time_buffer << "] ";
								std::cout << "ERROR: Any capture completed during the timeout time!";
								std::cout << std::endl;	
							
								return states::ACQUISITION_PUBLISH_STATUS;
						}
						
						HowIsPico = ps5000aStop(global_status.ps5000_handle);
						if (HowIsPico != PICO_OK)
						{
								char time_buffer[BUFFER_SIZE];
								time_string(time_buffer, BUFFER_SIZE, NULL);
								std::cout << '[' << time_buffer << "] ";
								std::cout << "ERROR: Not able to stop the acquisition!";
								std::cout << std::endl;		
								
								return states::ACQUISITION_ERROR;
						}
						
						// Get the definitive number of captures completed
						HowIsPico = ps5000aGetNoOfCaptures(global_status.ps5000_handle, &CompletedCaptures);
						if (HowIsPico != PICO_OK)
						{
								char time_buffer[BUFFER_SIZE];
								time_string(time_buffer, BUFFER_SIZE, NULL);
								std::cout << '[' << time_buffer << "] ";
								std::cout << "ERROR: Not able to get the number of completed captures!";
								std::cout << std::endl;		
								
								return states::ACQUISITION_ERROR;
						}
        }
        
        if (verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Downloading " << CompletedCaptures << " samples; ";
            std::cout << std::endl;
        }
        
        uint32_t true_Nsamples = global_status.Nsamples;
        HowIsPico = ps5000aGetValuesBulk(global_status.ps5000_handle,
																				 &true_Nsamples,
																				 0,
																				 CompletedCaptures - 1,
																				 1,
																				 PS5000A_RATIO_MODE_NONE,
																				 global_status.overflow); // Array of flags that indicate whether an overvoltage has occurred
                                      
        if (HowIsPico != PICO_OK)
				{
						char time_buffer[BUFFER_SIZE];
						time_string(time_buffer, BUFFER_SIZE, NULL);
						std::cout << '[' << time_buffer << "] ";
						std::cout << "ERROR: Not able to get values bulk!; ";
						std::cout << std::endl;
        
						return states::ACQUISITION_ERROR;
				}
				
				if( true_Nsamples != (uint32_t)global_status.Nsamples )
				{		
						char time_buffer[BUFFER_SIZE];
						time_string(time_buffer, BUFFER_SIZE, NULL);
						std::cout << '[' << time_buffer << "] ";
						std::cout << "ERROR: The requested number of samples (" << global_status.Nsamples << ") are not the same as the samples retrieved (" << true_Nsamples << ")!; ";
						std::cout << std::endl;
        
						return states::ACQUISITION_ERROR;
				}
				
				// Get timestamp infos for each segment. In one segment there is one waveform for each channel.
				std::vector<PS5000A_TRIGGER_INFO> trigger_info (CompletedCaptures);
				HowIsPico = ps5000aGetTriggerInfoBulk(global_status.ps5000_handle, 
																							trigger_info.data(), 
																							0,										
																							CompletedCaptures -1);
				
				if (HowIsPico != PICO_OK)
				{
						char time_buffer[BUFFER_SIZE];
						time_string(time_buffer, BUFFER_SIZE, NULL);
						std::cout << '[' << time_buffer << "] ";
						std::cout << "ERROR: Not able to get trigger info bulk!; ";
						std::cout << std::endl;
        
						return states::ACQUISITION_ERROR;
				}
				
				// We estimate total acq times, one block acq time and time from last bsl check.
        const auto Delta = std::chrono::duration_cast<nanoseconds>(block_end_time - global_status.start_time);
        const auto Delta_block = std::chrono::duration_cast<std::chrono::microseconds>(block_end_time - global_status.block_start_time);
        const auto Delta_bsl_check = std::chrono::duration_cast<std::chrono::seconds>(block_end_time - global_status.last_baseline_check);
        // We divide by timebase so we can have the same granularity as the samples in the waveform
        // because the minimum sample spacing is global_status.timeIntervalNs ns.
        const uint64_t timestampDelta = Delta.count() / global_status.timeIntervalNs;
        const uint64_t timeDelta_block = Delta_block.count();
        const uint64_t timeDelta_bsl_check = Delta_bsl_check.count();
				uint64_t InitialTimeStamp = trigger_info.at(0).timeStampCounter;

        if (verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            //std::cout << "Downloading samples from channel: " << channel << "; ";
            std::cout << "Acq time : " << timestampDelta << "; ";
            std::cout << "Block acq time: " << timeDelta_block << "; ";
            std::cout << "Last baseline check: " << timeDelta_bsl_check << "; ";
            std::cout << std::endl;
        }
            
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        FILE *pf = fopen("check_rate.txt","a");
        fprintf(pf,"%s\t%llu\t%llu\t%u\t%d",time_buffer,timestampDelta,timeDelta_block,CompletedCaptures,poll_success);
        //fclose(pf);
				
				// We have events... 
        for (auto &channel: global_status.enabled_channels)
        {
            uint64_t timestamp;
            uint32_t capture;
						double previous_bsl;

            if (verbosity > 0)
            {
                char time_buffer[BUFFER_SIZE];
                time_string(time_buffer, BUFFER_SIZE, NULL);
                std::cout << '[' << time_buffer << "] ";
                std::cout << "Downloading samples from channel: " << channel << "; ";
                std::cout << std::endl;
            }
            
            global_status.counts[channel] += CompletedCaptures;
            global_status.partial_counts[channel] += CompletedCaptures;
            
            if ( timeDelta_bsl_check > defaults_abps5000a_baseline_check_timeout || global_status.BlockNumber == 1 ) 
            {
								if (verbosity > 0)
								{
										char time_buffer[BUFFER_SIZE];
										time_string(time_buffer, BUFFER_SIZE, NULL);
										std::cout << '[' << time_buffer << "] ";
										std::cout << "It is time for a baseline check";
										std::cout << std::endl;
								}
								previous_bsl = global_status.baseline_check_values[channel];
								global_status.baseline_check_values[channel] = 0.;
								global_status.baseline_check_flag = true;
						}
						else global_status.baseline_check_flag = false;
						
						for (capture = 0; capture < CompletedCaptures; capture++) 
						{
								timestamp = timestampDelta + ( trigger_info.at(capture).timeStampCounter - InitialTimeStamp );
								
								global_status.waveforms_buffer.emplace_back(timestamp,
																														channel,
																														global_status.Nsamples);
						
								// Trasforming the signed int to an unsigned int for compatibility with the rest of ABCD
								// ADDED: baseline check -> After 60 s, it estimates the baseline. If it is changed significantly, re-arm triggers
								for (int j = 0; j < global_status.Nsamples; j++)
								{
										if ( global_status.baseline_check_flag && j < global_status.NpretriggerSamples*9/10 ) 
										{
												global_status.baseline_check_values[channel] += global_status.rapidBuffers[channel][capture][j];
										}
										global_status.waveforms_buffer.back().samples[j] = global_status.rapidBuffers[channel][capture][j] + (1 << 15);
								}
						}
						
						if ( global_status.baseline_check_flag )
						{
								global_status.baseline_check_values[channel] = global_status.baseline_check_values[channel] / ( global_status.NpretriggerSamples*9/10 * CompletedCaptures );
								const std::chrono::time_point<std::chrono::system_clock> baseline_check_time = std::chrono::system_clock::now();
								global_status.last_baseline_check = baseline_check_time;
								
								if (verbosity > 0)
								{
										char time_buffer[BUFFER_SIZE];
										time_string(time_buffer, BUFFER_SIZE, NULL);
										std::cout << '[' << time_buffer << "] ";
										std::cout << "Baseline value for channel " << channel << ": " << global_status.baseline_check_values[channel] << "; ";
										std::cout << "The previous values was: " << previous_bsl << "; ";
										std::cout << std::endl;
								}
								
								if ( global_status.baseline_check_values[channel] - previous_bsl > 5 || global_status.baseline_check_values[channel] - previous_bsl < -5 )
								{
										global_status.trigger_reconfiguration_should_I[channel] = true;
										
										if (verbosity > 0)
										{
												char time_buffer[BUFFER_SIZE];
												time_string(time_buffer, BUFFER_SIZE, NULL);
												std::cout << '[' << time_buffer << "] ";
												std::cout << "A reconfiguration of the trigger for channel " << channel << " is needed; ";
												std::cout << std::endl;
										}
								}
								else global_status.trigger_reconfiguration_should_I[channel] = false;	
						}		
						
						fprintf(pf,"\t%f\t%d",global_status.baseline_check_values[channel],global_status.trigger_values[channel] + (int16_t)global_status.baseline_check_values[channel]);
        }
				
				fprintf(pf,"\n");
				fclose(pf);

        if (verbosity > 0)
        {
            char time_buffer[BUFFER_SIZE];
            time_string(time_buffer, BUFFER_SIZE, NULL);
            std::cout << '[' << time_buffer << "] ";
            std::cout << "Waveforms buffer size: " << global_status.waveforms_buffer.size() << "; ";
            std::cout << std::endl;
        }
				
				if ( global_status.baseline_check_flag ) return states::TRIGGER_RECONFIGURATION;
				else return states::PUBLISH_EVENTS;
    }
    
    return states::ADD_TO_BUFFER;
}

state actions::publish_events(status &global_status)
{
    actions::generic::publish_events(global_status);

    return states::ACQUISITION_PUBLISH_STATUS;
    
}

state actions::stop_publish_events(status &global_status)
{
    actions::generic::publish_events(global_status);

    return states::STOP_ACQUISITION;
}

state actions::acquisition_publish_status(status &global_status)
{
    PICO_STATUS HowIsPico;
    short ZeroFlag = 0;
    HowIsPico = ps5000aPingUnit(global_status.ps5000_handle);

    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";

        if (HowIsPico == PICO_OK)
        {
            std::cout << "Picoscope status: Ok; ";
        }
        else if (HowIsPico == PICO_BUSY)
        {
            std::cout << "Picoscope status: Busy; ";
            std::cout << "Zero capture during the last 3 seconds, let's wait other 3 seconds; ";
        }
        else if (HowIsPico == PICO_NOT_RESPONDING)
        {
            std::cout << "Picoscope status: Not responding; ";
        }
        else
        {
            std::cout << "Picoscope status: " << HowIsPico << "; ";
        }
        std::cout << std::endl;
    }

    json_t *status_message = json_object();

    json_object_set_new_nocheck(status_message, "config", json_deep_copy(global_status.config));

    json_t *acquisition = json_object();
    json_t *digitizer = json_object();

    if (HowIsPico != PICO_OK && HowIsPico != PICO_BUSY)
    {
        json_object_set_new_nocheck(digitizer, "valid_pointer", json_false());
        json_object_set_new_nocheck(digitizer, "active", json_false());
        
        return states::ACQUISITION_ERROR;
    }
    else
    {
        json_object_set_new_nocheck(digitizer, "valid_pointer", json_true());
        json_object_set_new_nocheck(digitizer, "active", json_true());

        json_object_set_new_nocheck(acquisition, "running", json_true());

        const auto now = std::chrono::system_clock::now();

        const auto run_delta_time = std::chrono::duration_cast<std::chrono::duration<long int>>(now - global_status.start_time);
        const long int runtime = run_delta_time.count();

        json_object_set_new_nocheck(acquisition, "runtime", json_integer(runtime));

        const auto pub_delta_time = std::chrono::duration_cast<nanoseconds>(now - global_status.block_start_time);
        const double pubtime = static_cast<double>(pub_delta_time.count() * 1E-9);
        //std::cout << "pubtime: " << pubtime << "; ";
				//std::cout << std::endl;
				
        json_t *rates = json_array();
        json_t *ICR_rates = json_array();

        json_t *counts = json_array();
        json_t *ICR_counts = json_array();

        if (pubtime > 0)
        {
            for (unsigned int i = 0; i < global_status.TotNchannel; i++)
						{
                const double rate = global_status.partial_counts[i] / pubtime;
                json_array_append_new(rates, json_real(rate));
                json_array_append_new(ICR_rates, json_real(rate));
            }
        }
        else
        {
            for (unsigned int i = 0; i < global_status.TotNchannel; i++)
						{
                json_array_append_new(rates, json_integer(0));
                json_array_append_new(ICR_rates, json_integer(0));
            }
        }
				
        for (unsigned int i = 0; i < global_status.TotNchannel; i++)
        {
            const unsigned int channel_counts = global_status.counts[i];
            json_array_append_new(counts, json_integer(channel_counts));
            json_array_append_new(ICR_counts, json_integer(channel_counts));
            if(channel_counts == 0) ZeroFlag ++;
        }

        json_object_set_new_nocheck(acquisition, "rates", rates);
        json_object_set_new_nocheck(acquisition, "ICR_rates", ICR_rates);

        json_object_set_new_nocheck(acquisition, "counts", counts);
        json_object_set_new_nocheck(acquisition, "ICR_counts", ICR_counts);
    }

    json_object_set_new_nocheck(status_message, "acquisition", acquisition);
    json_object_set_new_nocheck(status_message, "digitizer", digitizer);

    actions::generic::publish_message(global_status, defaults_abcd_status_topic, status_message);

    json_decref(status_message);
    
    if (ZeroFlag == global_status.TotNchannel) 
    {
				return states::ACQUISITION_RECEIVE_COMMANDS; //If no events, nothing have been published and the acquisition is still running
		}
		else 
		{
				// Clear event partial counts
				for (unsigned int i = 0; i < global_status.TotNchannel; i++)
				{
						global_status.partial_counts[i] = 0;
				}

				return states::CONTINUE_ACQUISITION; //START ANOTHER RUN BLOCK
		}
}

state actions::continue_acquisition(status &global_status)
{
		PICO_STATUS HowIsPico;
		
    HowIsPico = ps5000aPingUnit(global_status.ps5000_handle);
    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";

        if (HowIsPico == PICO_OK)
        {
            std::cout << "Picoscope status: Ok; ";
        }
        else if (HowIsPico == PICO_BUSY)
        {
            std::cout << "Picoscope status: Busy; ";
        }
        else if (HowIsPico == PICO_NOT_RESPONDING)
        {
            std::cout << "Picoscope status: Not responding; ";
        }
        else
        {
            std::cout << "Picoscope status: " << HowIsPico << "; ";
        }
        std::cout << std::endl;
    }
        
    if(HowIsPico != PICO_OK)
    {
				char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Pico is not ok right now!";
        std::cout << std::endl;
    
        return states::ACQUISITION_ERROR;
    }

		// Start acquisition of another block
    std::chrono::time_point<std::chrono::system_clock> block_start_time = std::chrono::system_clock::now();
    
    HowIsPico = ps5000aRunBlock( global_status.ps5000_handle, 
																 global_status.NpretriggerSamples, 
																 global_status.Nsamples - global_status.NpretriggerSamples, 
																 global_status.timebase, 
																 NULL, 
																 0, 
																 NULL, 
																 NULL );
																 
	  if (HowIsPico != PICO_OK)
    {
				char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Not able to start the acquisition!";
        std::cout << std::endl;		
        
        return states::ACQUISITION_ERROR;
		}
		
		global_status.BlockNumber++;
		
		if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "Acquisition of block " << global_status.BlockNumber << " started.";
        std::cout << std::endl;
    }
		
		global_status.block_start_time = block_start_time;

    return states::ACQUISITION_RECEIVE_COMMANDS;
}

state actions::trigger_reconfiguration(status &global_status)
{
		PICO_STATUS HowIsPico;
		int16_t new_trigger_value;
		std::vector<PS5000A_TRIGGER_CHANNEL_PROPERTIES_V2> triggerProperties;
		
    // Reconfigure trigger threshold due to baseline shift
		for (auto &channel: global_status.enabled_channels)
		{
				if ( global_status.trigger_reconfiguration_should_I[channel] )
				{
						// What is the channel?
						PS5000A_CHANNEL en_ch = PS5000A_CHANNEL_A;
						if(channel == 0) en_ch = PS5000A_CHANNEL_A;
						else if(channel == 1) en_ch = PS5000A_CHANNEL_B;
						
						// Calculate new trigger threshold
						new_trigger_value = global_status.trigger_values[channel] + global_status.baseline_check_values[channel];		
						
						// If only 1 channel enabled, simple trigger
						if( global_status.NchannelsEnabled == 1 )
						{								
								// Let's change the trigger
								HowIsPico = ps5000aSetSimpleTrigger(global_status.ps5000_handle, 
																						1, 
																						en_ch, 
																						new_trigger_value, 
																						global_status.ps5000_1ch_trigger_dir, 
																						0, 
																						0);
								if (HowIsPico != PICO_OK)
								{
										char time_buffer[BUFFER_SIZE];
										time_string(time_buffer, BUFFER_SIZE, NULL);
										std::cout << '[' << time_buffer << "] ";
										std::cout << "ERROR: Not able to reconfigure the trigger for the only channel enabled! ";
										std::cout << std::endl;

										return states::ACQUISITION_ERROR;
								}								
						}
            else if ( global_status.NchannelsEnabled > 1 )
            {								
								triggerProperties.push_back({new_trigger_value, 0, 0, 0, en_ch});
            }
				
						if (global_status.verbosity > 0)
						{
								char time_buffer[BUFFER_SIZE];
								time_string(time_buffer, BUFFER_SIZE, NULL);
								std::cout << '[' << time_buffer << "] ";
								std::cout << "Original threshold for channel " << channel << ": " << global_status.trigger_values[channel] << "; ";
								std::cout << "New trigger value: " << new_trigger_value << "; ";
								std::cout << std::endl;
						}
				
				}
		
		}
		
		if (global_status.NchannelsEnabled > 1)
		{
				int16_t auxOutputEnable = 0; // not used
				HowIsPico = ps5000aSetTriggerChannelPropertiesV2( global_status.ps5000_handle, 
																													triggerProperties.data(), 
																													triggerProperties.size(), 
																													auxOutputEnable);
				if (HowIsPico != PICO_OK)
				{
						char time_buffer[BUFFER_SIZE];
						time_string(time_buffer, BUFFER_SIZE, NULL);
						std::cout << '[' << time_buffer << "] ";
						std::cout << "ERROR: Not able to reconfigure the channels trigger properties; ";
						std::cout << std::endl;

						return states::ACQUISITION_ERROR;
				}
		}
															 
    return states::PUBLISH_EVENTS;
}

state actions::stop_acquisition(status &global_status)
{
    actions::generic::stop_acquisition(global_status);

    auto delta_time = std::chrono::duration_cast<std::chrono::duration<long int>>(global_status.stop_time - global_status.start_time);
    const std::string event_message = "Stop acquisition (duration: " + std::to_string(delta_time.count()) + " s)";

    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string(event_message.c_str()));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::RECEIVE_COMMANDS;
}

state actions::clear_memory(status &global_status)
{
    PICO_STATUS HowIsPico;
    HowIsPico = ps5000aPingUnit(global_status.ps5000_handle);
    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";

        if (HowIsPico == PICO_OK)
        {
            std::cout << "Picoscope status: Ok; ";
        }
        else if (HowIsPico == PICO_BUSY)
        {
            std::cout << "Picoscope status: Busy; ";
        }
        else if (HowIsPico == PICO_NOT_RESPONDING)
        {
            std::cout << "Picoscope status: Not responding; ";
        }
        else
        {
            std::cout << "Picoscope status: " << HowIsPico << "; ";
        }
        std::cout << std::endl;
    }
        
    if(HowIsPico == PICO_OK)
    {
				actions::generic::clear_memory(global_status);
    }
    else
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Pico is not ok right now!";
        std::cout << std::endl;
    }

    return states::DESTROY_DIGITIZER;
}

state actions::reconfigure_clear_memory(status &global_status)
{
    PICO_STATUS HowIsPico;
    HowIsPico = ps5000aPingUnit(global_status.ps5000_handle);
    if (global_status.verbosity > 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";

        if (HowIsPico == PICO_OK)
        {
            std::cout << "Picoscope status: Ok; ";
        }
        else if (HowIsPico == PICO_BUSY)
        {
            std::cout << "Picoscope status: Busy; ";
        }
        else if (HowIsPico == PICO_NOT_RESPONDING)
        {
            std::cout << "Picoscope status: Not responding; ";
        }
        else
        {
            std::cout << "Picoscope status: " << HowIsPico << "; ";
        }
        std::cout << std::endl;
    }
        
    if(HowIsPico == PICO_OK)
    {
				actions::generic::clear_memory(global_status);
    }
    else
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ERROR: Pico is not ok right now!";
        std::cout << std::endl;
    
        return states::CONFIGURE_ERROR;
    }

    return states::RECONFIGURE_DESTROY_DIGITIZER;
}

state actions::destroy_digitizer(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string("Digitizer deactivation"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    actions::generic::destroy_digitizer(global_status);

    return states::CLOSE_SOCKETS;
}

state actions::restart_publish_events(status &global_status)
{
    actions::generic::publish_events(global_status);

    return states::RESTART_STOP_ACQUISITION;
}

state actions::restart_stop_acquisition(status &global_status)
{
    actions::generic::stop_acquisition(global_status);

    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("event"));
    json_object_set_new_nocheck(json_event_message, "event", json_string("Restart stop acquisition"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::RESTART_CLEAR_MEMORY;
}

state actions::restart_clear_memory(status &global_status)
{
    actions::generic::clear_memory(global_status);
    
    return states::RESTART_DESTROY_DIGITIZER;
}

state actions::restart_destroy_digitizer(status &global_status)
{
    actions::generic::destroy_digitizer(global_status);

    return states::RESTART_CREATE_DIGITIZER;
}

state actions::restart_create_digitizer(status &global_status)
{
    const bool success = actions::generic::create_digitizer(global_status);

    if (success)
    {
        return states::RESTART_CONFIGURE_DIGITIZER;
    }
    else
    {
        return states::RESTART_CONFIGURE_ERROR;
    }
}

state actions::restart_configure_digitizer(status &global_status)
{
    const bool success = actions::generic::configure_digitizer(global_status);

    if (success)
    {
        return states::RESTART_ALLOCATE_MEMORY;
    }
    else
    {
        return states::RESTART_CONFIGURE_ERROR;
    }
}

state actions::close_sockets(status &global_status)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));

    void *status_socket = global_status.status_socket;
    void *data_socket = global_status.data_socket;
    void *commands_socket = global_status.commands_socket;

    const int s = zmq_close(status_socket);
    if (s != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on status socket close: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    const int d = zmq_close(data_socket);
    if (d != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on data socket close: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    const int c = zmq_close(commands_socket);
    if (c != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on commands socket close: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    return states::DESTROY_CONTEXT;
}

state actions::destroy_context(status &global_status)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(defaults_abcd_zmq_delay));

    void *context = global_status.context;

    const int c = zmq_ctx_destroy(context);
    if (c != 0)
    {
        char time_buffer[BUFFER_SIZE];
        time_string(time_buffer, BUFFER_SIZE, NULL);
        std::cout << '[' << time_buffer << "] ";
        std::cout << "ZeroMQ Error on context destroy: ";
        std::cout << zmq_strerror(errno);
        std::cout << std::endl;
    }

    return states::STOP;
}

state actions::communication_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Communication error"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::CLOSE_SOCKETS;
}

state actions::parse_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Config parse error"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::CLOSE_SOCKETS;
}

state actions::configure_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Configure error"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::RECONFIGURE_DESTROY_DIGITIZER;
}

state actions::digitizer_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Digitizer error"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::RECONFIGURE_CLEAR_MEMORY;
}

state actions::acquisition_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Acquisition error"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::RESTART_PUBLISH_EVENTS;
}

state actions::restart_configure_error(status &global_status)
{
    json_t *json_event_message = json_object();

    json_object_set_new_nocheck(json_event_message, "type", json_string("error"));
    json_object_set_new_nocheck(json_event_message, "error", json_string("Restart configure error"));

    actions::generic::publish_message(global_status, defaults_abcd_events_topic, json_event_message);

    json_decref(json_event_message);

    return states::RESTART_DESTROY_DIGITIZER;
}
