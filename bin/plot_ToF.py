#! /usr/bin/env python3

#  (C) Copyright 2016 Cristiano Lino Fontana
#
#  This file is part of ABCD.
#
#  ABCD is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  ABCD is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with ABCD.  If not, see <http://www.gnu.org/licenses/>.

import argparse
import os
import datetime

import math

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.colors import LogNorm

NS_PER_SAMPLE = 2.0 / 1024
TIME_RESOLUTION = 2.0
TIME_MIN = -200
TIME_MAX = 200
ENERGY_RESOLUTION = 20
ENERGY_MIN = 0
ENERGY_MAX = 20000
EVENTS_COUNT = -1

parser = argparse.ArgumentParser(description='Reads an ABCD events file and plots the ToF between two channels')
parser.add_argument('file_name',
                    type = str,
                    help = 'Input file name')
parser.add_argument('channel_a',
                    type = int,
                    help = 'Channel selection')
parser.add_argument('channel_b',
                    type = int,
                    help = 'Channel selection')
parser.add_argument('-n',
                    '--ns_per_sample',
                    type = float,
                    default = NS_PER_SAMPLE,
                    help = 'Nanoseconds per sample (default: {:f})'.format(NS_PER_SAMPLE))
parser.add_argument('-N',
                    '--events_count',
                    type = int,
                    default = EVENTS_COUNT,
                    help = 'Max events to be read from file (default: {:f})'.format(EVENTS_COUNT))
parser.add_argument('-r',
                    '--time_resolution',
                    type = float,
                    default = TIME_RESOLUTION,
                    help = 'Time resolution (default: {:f})'.format(TIME_RESOLUTION))
parser.add_argument('-t',
                    '--time_min',
                    type = float,
                    default = TIME_MIN,
                    help = 'Time min (default: {:f})'.format(TIME_MIN))
parser.add_argument('-T',
                    '--time_max',
                    type = float,
                    default = TIME_MAX,
                    help = 'Time max (default: {:f})'.format(TIME_MAX))
parser.add_argument('-R',
                    '--energy_resolution',
                    type = float,
                    default = ENERGY_RESOLUTION,
                    help = 'Energy resolution (default: {:f})'.format(ENERGY_RESOLUTION))
parser.add_argument('-e',
                    '--energy_min',
                    type = float,
                    default = ENERGY_MIN,
                    help = 'Energy min (default: {:f})'.format(ENERGY_MIN))
parser.add_argument('-E',
                    '--energy_max',
                    type = float,
                    default = ENERGY_MAX,
                    help = 'Energy max (default: {:f})'.format(ENERGY_MAX))
parser.add_argument('-s',
                    '--save_data',
                    action = "store_true",
                    help = 'Save histograms to file')

args = parser.parse_args()

start_time = datetime.datetime.now()

channel_a = args.channel_a
channel_b = args.channel_b

time_min = args.time_min
time_max = args.time_max

print("Time min: {:f}".format(time_min))
print("Time max: {:f}".format(time_max))

time_resolution = args.time_resolution
N_t = math.ceil((time_max - time_min)/ time_resolution)

print("N_t: {:d}".format(N_t))

energy_min = args.energy_min
energy_max = args.energy_max

print("Energy min: {:f}".format(energy_min))
print("Energy max: {:f}".format(energy_max))

energy_resolution = args.energy_resolution
N_E = math.ceil((energy_max - energy_min)/ energy_resolution)

print("N_E: {:d}".format(N_E))

event_PSD_dtype = np.dtype([('timestamp', np.uint64),
                            ('qshort', np.uint16),
                            ('qlong', np.uint16),
                            ('baseline', np.uint16),
                            ('channel', np.uint8),
                            ('pur', np.uint8),
                            ])

print("### ### Reading: {}".format(args.file_name))
data = np.fromfile(args.file_name, dtype = event_PSD_dtype, count = args.events_count)

print("Selecting channels...")
channels_selection = np.logical_or(data['channel'] == args.channel_a, data['channel'] == args.channel_b)
selected_data = data[channels_selection]

print("Sorting data...")
sorted_data = np.sort(selected_data, order = 'timestamp')

channels = sorted_data['channel']
timestamps = sorted_data['timestamp'] * args.ns_per_sample
energies = sorted_data['qlong']

total_events = len(timestamps)

min_time = min(timestamps)
max_time = max(timestamps)

Delta_time = (max_time - min_time) * args.ns_per_sample * 1e-9

print("Number of events: {:d}".format(total_events))
print("Time delta: {:f} s".format(Delta_time))
print("Average rate: {:f} Hz".format(total_events / Delta_time))
                                                

print("Starting the main loop for {:d} events...".format(total_events))

selected_events = 0

time_differences = list()
coincidence_energies_a = list()
coincidence_energies_a_with_repetitions = list()
coincidence_energies_b = list()

try:
    for this_index, (this_channel, this_timestamp, this_energy) in enumerate(zip(channels, timestamps, energies)):

        if this_channel == channel_a:
            select_energy_a = False

            left_edge = time_min + this_timestamp
            right_edge = time_max + this_timestamp

            for that_index in range(this_index + 1, total_events):
                that_channel = channels[that_index]
                that_timestamp = timestamps[that_index]
                that_energy = energies[that_index]

                if left_edge < that_timestamp and that_timestamp < right_edge and \
                   energy_min < this_energy and this_energy < energy_max and \
                   energy_min < that_energy and that_energy < energy_max:
                    if that_channel == channel_b:
                        selected_events += 1
                        time_differences.append(that_timestamp - this_timestamp)
                        coincidence_energies_b.append(that_energy)
                        coincidence_energies_a_with_repetitions.append(this_energy)
                        select_energy_a = True

                        print("difference: {:6.1f}; selected_events: {:d} / {:d} ({:.2f}%); index: {:d}/{:d} ({:.2f}%)".format(that_timestamp - this_timestamp, selected_events, total_events, selected_events / float(this_index + 1) * 100, this_index, total_events, this_index / float(total_events) * 100))
                else:
                    break

            for that_index in range(this_index - 1, -1, -1):
                that_timestamp = timestamps[that_index]
                that_channel = channels[that_index]
                that_energy = energies[that_index]

                if left_edge < that_timestamp and that_timestamp < right_edge and \
                   energy_min < this_energy and this_energy < energy_max and \
                   energy_min < that_energy and that_energy < energy_max:
                    if that_channel == channel_b:
                        selected_events += 1
                        time_differences.append(that_timestamp - this_timestamp)
                        coincidence_energies_b.append(that_energy)
                        coincidence_energies_a_with_repetitions.append(this_energy)
                        select_energy_a = True

                        print("difference: {:6.1f}; selected_events: {:d} / {:d} ({:.2f}%); index: {:d}/{:d} ({:.2f}%)".format(that_timestamp - this_timestamp, selected_events, total_events, selected_events / float(this_index + 1) * 100, this_index, total_events, this_index / float(total_events) * 100))
                else:
                    break

            if select_energy_a:
                # Also the event from the reference channel is selected and needs to be counted
                selected_events += 1
                coincidence_energies_a.append(this_energy)
except KeyboardInterrupt:
    pass

stop_time = datetime.datetime.now()

print("Total time: {}; time per event: {:f} µs".format(stop_time - start_time, (stop_time - start_time).total_seconds() * 1e6 / total_events))

ToF_histo, ToF_edges = np.histogram(time_differences, bins = N_t, range = (time_min, time_max))
E_histo_a, E_edges_a = np.histogram(coincidence_energies_a, bins = N_E, range = (energy_min, energy_max))
E_histo_b, E_edges_b = np.histogram(coincidence_energies_b, bins = N_E, range = (energy_min, energy_max))
EvsToF_histo_a, E_edges_a, ToF_edges = np.histogram2d(coincidence_energies_a, time_differences,
                                                      bins = (N_E, N_t),
                                                      range = ((energy_min, energy_max), (time_min, time_max)))
EvsToF_histo_b, E_edges_b, ToF_edges = np.histogram2d(coincidence_energies_b, time_differences,
                                                      bins = (N_E, N_t),
                                                      range = ((energy_min, energy_max), (time_min, time_max)))
EvsE_histo, E_edges_a, E_edges_b = np.histogram2d(coincidence_energies_a_with_repetitions,
                                                  coincidence_energies_b,
                                                  bins = (N_E, N_E),
                                                  range = ((energy_min, energy_max), (energy_min, energy_max)))

# Normalizing histograms to counts per second
ToF_histo = ToF_histo / Delta_time
E_histo_a = E_histo_a / Delta_time
E_histo_b = E_histo_b / Delta_time

if args.save_data:
    basename, extension = os.path.splitext(args.file_name)
    extension = '.csv'

    output_file_name = basename + '_Ch{}andCh{}_ToF-histo'.format(channel_a, channel_b) + extension
    output_array = np.vstack((ToF_edges[:-1], ToF_histo)).T

    print("Writing ToF histogram to: {}".format(output_file_name))
    np.savetxt(output_file_name, output_array)

    output_file_name = basename + '_Ch{}andCh{}_E-histo_Ch{}'.format(channel_a, channel_b, channel_a) + extension
    output_array = np.vstack((E_edges_a[:-1], E_histo_a)).T

    print("Writing E histogram to: {}".format(output_file_name))
    np.savetxt(output_file_name, output_array)

    output_file_name = basename + '_Ch{}andCh{}_E-histo_Ch{}'.format(channel_a, channel_b, channel_b) + extension
    output_array = np.vstack((E_edges_b[:-1], E_histo_b)).T

    print("Writing E histogram to: {}".format(output_file_name))
    np.savetxt(output_file_name, output_array)

    extension = '.txt'

    output_file_name = basename + '_Ch{}andCh{}_ToF-E_values'.format(channel_a, channel_b) + extension
    output_array = np.vstack((time_differences,
                              coincidence_energies_a_with_repetitions,
                              coincidence_energies_b)).T

    print("Writing ToF values to: {}".format(output_file_name))
    np.savetxt(output_file_name, output_array)

else:
    #plt.ion()

    fig = plt.figure()

    histo_t_ax = fig.add_subplot(111)

    histo_t_ax.step(ToF_edges[:-1], ToF_histo)
    histo_t_ax.set_xlabel('Time [ns]')
    histo_t_ax.grid()

    fig = plt.figure()
    fig.subplots_adjust(hspace = 0.5)

    histo_E_ax_a = fig.add_subplot(211)

    histo_E_ax_a.step(E_edges_a[:-1], E_histo_a)
    histo_E_ax_a.set_xlabel('Energy [ch]')
    histo_E_ax_a.grid()
    histo_E_ax_a.set_title("Reference channel: {}".format(channel_a))

    histo_E_ax_b = fig.add_subplot(212)

    histo_E_ax_b.step(E_edges_b[:-1], E_histo_b)
    histo_E_ax_b.set_xlabel('Energy [ch]')
    histo_E_ax_b.grid()
    histo_E_ax_b.set_title("Channel: {}".format(channel_b))

    fig_width, fig_height = fig.get_size_inches()

    fig = plt.figure(figsize = (fig_height * 2, fig_height))

    bihistos_ax_a = fig.add_subplot(121)

    cax = bihistos_ax_a.imshow(EvsToF_histo_a,
                               origin = 'lower',
                               norm = LogNorm(),
                               interpolation = 'none',
                               extent = (time_min, time_max, energy_min, energy_max),
                               aspect = 'auto')
    #cbar = fig.colorbar(cax)

    bihistos_ax_a.set_xlabel('Time [ns]')
    bihistos_ax_a.set_ylabel('Energy [ch]')
    bihistos_ax_a.grid()
    bihistos_ax_a.set_title("Reference channel: {}".format(channel_a))

    bihistos_ax_b = fig.add_subplot(122, sharex = bihistos_ax_a, sharey = bihistos_ax_a)

    cax = bihistos_ax_b.imshow(EvsToF_histo_b,
                               origin = 'lower',
                               norm = LogNorm(),
                               interpolation = 'none',
                               extent = (time_min, time_max, energy_min, energy_max),
                               aspect = 'auto')
    #cbar = fig.colorbar(cax)

    bihistos_ax_b.set_xlabel('Time [ns]')
    #bihistos_ax_b.set_ylabel('Energy [ch]')
    bihistos_ax_b.grid()
    bihistos_ax_b.set_title("Channel: {}".format(channel_b))

    fig = plt.figure(figsize = (fig_height, fig_height))
    fig.subplots_adjust(left = 0.18)

    bihisto_ax = fig.add_subplot(111)

    cax = bihisto_ax.imshow(EvsE_histo,
                            origin = 'lower',
                            norm = LogNorm(),
                            interpolation = 'none',
                            extent = (energy_min, energy_max, energy_min, energy_max),
                            aspect = 'auto')
    #cbar = fig.colorbar(cax)

    bihisto_ax.set_ylabel('Energy (reference channel {}) [ch]'.format(channel_a))
    bihisto_ax.set_xlabel('Energy (channel {}) [ch]'.format(channel_b))
    bihisto_ax.grid()

    plt.show()
