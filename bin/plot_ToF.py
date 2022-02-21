#! /usr/bin/env python3

#  (C) Copyright 2022, European Union, Cristiano Lino Fontana
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
import functools

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.colors import LogNorm

NS_PER_SAMPLE = 2.0 / 1024
TIME_RESOLUTION = 2.0
TIME_MIN = -200
TIME_MAX = 200
ENERGY_RESOLUTION = 20
ENERGY_MIN = 0
ENERGY_MAX = 66000
ToF_OFFSET = 0
# This should be 8 MB
BUFFER_SIZE = 8 * 1024 * 1024
IMAGES_EXTENSION = 'pdf'

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
parser.add_argument('--reference_energy_min',
                    type = float,
                    default = ENERGY_MIN,
                    help = 'Reference energy min (default: {:f})'.format(ENERGY_MIN))
parser.add_argument('--reference_energy_max',
                    type = float,
                    default = ENERGY_MAX,
                    help = 'Reference energy max (default: {:f})'.format(ENERGY_MAX))
parser.add_argument('-B',
                    '--buffer_size',
                    type = int,
                    default = BUFFER_SIZE,
                    help = 'Buffer size for file reading (default: {:f})'.format(BUFFER_SIZE))
parser.add_argument('-s',
                    '--save_data',
                    action = "store_true",
                    help = 'Save histograms to file')
parser.add_argument('--save_plots',
                    action = "store_true",
                    help = 'Save plots to file')
parser.add_argument('-m',
                    '--ToF_modulo',
                    default = None,
                    help = 'If set, the ToF is calculated modulo this value')
parser.add_argument('-o',
                    '--ToF_offset',
                    type = float,
                    default = ToF_OFFSET,
                    help = 'If a modulo is set, an offset added to the ToF in ns (default: {:f})'.format(ToF_OFFSET))
parser.add_argument('--images_extension',
                    type = str,
                    default = IMAGES_EXTENSION,
                    help = 'Define the extension of the image files (default: {})'.format(IMAGES_EXTENSION))

args = parser.parse_args()

event_PSD_dtype = np.dtype([('timestamp', np.uint64),
                            ('qshort', np.uint16),
                            ('qlong', np.uint16),
                            ('baseline', np.uint16),
                            ('channel', np.uint8),
                            ('pur', np.uint8),
                            ])

spectra = list()
spectra_derivatives = list()

buffer_size = args.buffer_size - (args.buffer_size % 16)
print("Using buffer size: {:d}".format(buffer_size))

energy_resolution = args.energy_resolution

energy_min = args.energy_min
energy_max = args.energy_max
N_E = math.floor((energy_max - energy_min)/ energy_resolution)

print("Energy min: {:f}".format(energy_min))
print("Energy max: {:f}".format(energy_max))
print("N_E: {:d}".format(N_E))

reference_energy_min = args.reference_energy_min
reference_energy_max = args.reference_energy_max
N_rE = math.floor((reference_energy_max - reference_energy_min)/ energy_resolution)

print("Reference energy min: {:f}".format(reference_energy_min))
print("Reference energy max: {:f}".format(reference_energy_max))
print("N_E: {:d}".format(N_rE))

time_min = args.time_min
time_max = args.time_max
time_resolution = args.time_resolution
N_t = math.floor((time_max - time_min)/ time_resolution)

print("Time min: {:f}".format(time_min))
print("Time max: {:f}".format(time_max))
print("N_t: {:d}".format(N_t))

try:
    ToF_modulo = float(args.ToF_modulo)
except:
    ToF_modulo = 0
ToF_offset = args.ToF_offset

print("Time modulo: {:f}".format(ToF_modulo))
print("Time offset: {:f}".format(args.ToF_offset))

channel_a = args.channel_a
channel_b = args.channel_b

partial_ToF_histo = np.zeros(N_t)
partial_E_histo_a = np.zeros(N_rE)
partial_E_histo_b = np.zeros(N_E)
partial_EvsToF_histo_a = np.zeros((N_rE, N_t))
partial_EvsToF_histo_b = np.zeros((N_E, N_t))
partial_EvsE_histo = np.zeros((N_rE, N_E))

min_times = list()
max_times = list()
ToF_edges = None
E_edges_a = None
E_edges_b = None
counter = 0
events_counter = 0

time_differences = list()
coincidence_energies_a = list()
coincidence_energies_a_with_repetitions = list()
coincidence_energies_b = list()


# We will read the file in chunks so we can process also very big files
# This means that we will lose the coincidences between chunks
with open(args.file_name, "rb") as input_file:
    while True:
        start_time = datetime.datetime.now()

        try:
            print("### ### Reading chunk: {:d}".format(counter))

            file_chunk = input_file.read(buffer_size)

            data = np.frombuffer(file_chunk, dtype = event_PSD_dtype)

            print("Selecting channels...")
            channels_selection = np.logical_or(data['channel'] == args.channel_a, \
                                               data['channel'] == args.channel_b)
            selected_data = data[channels_selection]

            print("Sorting data...")
            sorted_data = np.sort(selected_data, order = 'timestamp')

            channels = sorted_data['channel']
            timestamps = sorted_data['timestamp'] * args.ns_per_sample
            energies = sorted_data['qlong']

            total_events = len(timestamps)

            events_counter += total_events

            min_time = min(timestamps)
            max_time = max(timestamps)

            min_times.append(min_time)
            max_times.append(max_time)

            Delta_time = (max_time - min_time) * args.ns_per_sample * 1e-9

            print("Number of events: {:d}".format(total_events))
            print("Time delta: {:f} s".format(Delta_time))
            print("Average rate: {:f} Hz".format(total_events / Delta_time))

            print("Starting the main loop for {:d} events...".format(total_events))

            selected_events = 0

            for this_index, (this_channel, this_timestamp, this_energy) in enumerate(zip(channels, timestamps, energies)):

                if this_channel == channel_a and \
                   reference_energy_min < this_energy and this_energy < reference_energy_max:
                    select_energy_a = False

                    left_edge = time_min + this_timestamp
                    right_edge = time_max + this_timestamp

                    for that_index in range(this_index + 1, total_events):
                        that_channel = channels[that_index]
                        that_timestamp = timestamps[that_index]
                        that_energy = energies[that_index]

                        if left_edge < that_timestamp and that_timestamp < right_edge:
                            if energy_min < that_energy and that_energy < energy_max and \
                               that_channel == channel_b:

                                selected_events += 1
                                if args.ToF_modulo is None:
                                    time_difference = that_timestamp - this_timestamp
                                else:
                                    time_difference = (that_timestamp - this_timestamp + ToF_offset) % ToF_modulo

                                time_differences.append(time_difference)
                                coincidence_energies_b.append(that_energy)
                                coincidence_energies_a_with_repetitions.append(this_energy)
                                select_energy_a = True

                                print("difference: {:6.1f}; selected_events: {:d} / {:d} ({:.2f}%); index: {:d}/{:d} ({:.2f}%)".format(time_difference, selected_events, this_index, selected_events / float(this_index + 1) * 100, this_index, total_events, this_index / float(total_events) * 100))
                        else:
                            break

                    for that_index in range(this_index - 1, -1, -1):
                        that_timestamp = timestamps[that_index]
                        that_channel = channels[that_index]
                        that_energy = energies[that_index]

                        if left_edge < that_timestamp and that_timestamp < right_edge:
                            if energy_min < that_energy and that_energy < energy_max and \
                               that_channel == channel_b:

                                selected_events += 1
                                if args.ToF_modulo is None:
                                    time_difference = that_timestamp - this_timestamp
                                else:
                                    time_difference = (that_timestamp - this_timestamp + ToF_offset) % ToF_modulo

                                time_differences.append(time_difference)
                                coincidence_energies_b.append(that_energy)
                                coincidence_energies_a_with_repetitions.append(this_energy)
                                select_energy_a = True

                                print("difference: {:6.1f}; selected_events: {:d} / {:d} ({:.2f}%); index: {:d}/{:d} ({:.2f}%)".format(time_difference, selected_events, this_index, selected_events / float(this_index + 1) * 100, this_index, total_events, this_index / float(total_events) * 100))
                        else:
                            break

                    if select_energy_a:
                        # Also the event from the reference channel is selected and needs to be counted
                        selected_events += 1
                        coincidence_energies_a.append(this_energy)

            stop_time = datetime.datetime.now()

            print("Total time: {}; time per event: {:f} µs".format(stop_time - start_time, (stop_time - start_time).total_seconds() * 1e6 / total_events))

            ToF_histo, ToF_edges = \
                np.histogram(time_differences, bins = N_t, range = (time_min, time_max))
            E_histo_a, E_edges_a = \
                np.histogram(coincidence_energies_a, bins = N_rE, range = (reference_energy_min, reference_energy_max))
            E_histo_b, E_edges_b = \
                np.histogram(coincidence_energies_b, bins = N_E, range = (energy_min, energy_max))
            EvsToF_histo_a, E_edges_a, ToF_edges = \
                np.histogram2d(coincidence_energies_a_with_repetitions, time_differences,
                               bins = (N_rE, N_t),
                               range = ((reference_energy_min, reference_energy_max), (time_min, time_max)))
            EvsToF_histo_b, E_edges_b, ToF_edges = \
                np.histogram2d(coincidence_energies_b, time_differences,
                               bins = (N_E, N_t),
                               range = ((energy_min, energy_max), (time_min, time_max)))
            EvsE_histo, E_edges_a, E_edges_b = \
                np.histogram2d(coincidence_energies_a_with_repetitions,
                               coincidence_energies_b,
                               bins = (N_rE, N_E),
                               range = ((reference_energy_min, reference_energy_max), (energy_min, energy_max)))

            partial_ToF_histo += ToF_histo
            partial_E_histo_a += E_histo_a
            partial_E_histo_b += E_histo_b
            partial_EvsToF_histo_a += EvsToF_histo_a
            partial_EvsToF_histo_b += EvsToF_histo_b
            partial_EvsE_histo += EvsE_histo

            counter += 1

        except KeyboardInterrupt:
            break
        except Exception as error:
            break

min_time = min(min_times)
max_time = max(max_times)

Delta_time = (max_time - min_time) * args.ns_per_sample * 1e-9

print("    Number of events: {:d}".format(events_counter))
print("    Time delta: {:f} s".format(Delta_time))
print("    Average rate: {:f} Hz".format(events_counter / Delta_time))

ToF_histo = partial_ToF_histo
E_histo_a = partial_E_histo_a
E_histo_b = partial_E_histo_b
EvsToF_histo_a = partial_EvsToF_histo_a
EvsToF_histo_b = partial_EvsToF_histo_b
EvsE_histo = partial_EvsE_histo

basename, extension = os.path.splitext(args.file_name)

basename += '_Ch{}andCh{}'.format(channel_a, channel_b)

if args.save_data:
    extension = '.csv'

    output_file_name = basename + '_ToF-histo' + extension
    output_array = np.vstack((ToF_edges[:-1], ToF_histo)).T

    print("Writing ToF histogram to: {}".format(output_file_name))
    np.savetxt(output_file_name, output_array)

    output_file_name = basename + '_E-histo_Ch{}'.format(channel_a) + extension
    output_array = np.vstack((E_edges_a[:-1], E_histo_a)).T

    print("Writing E histogram to: {}".format(output_file_name))
    np.savetxt(output_file_name, output_array)

    output_file_name = basename + '_E-histo_Ch{}'.format(channel_b) + extension
    output_array = np.vstack((E_edges_b[:-1], E_histo_b)).T

    print("Writing E histogram to: {}".format(output_file_name))
    np.savetxt(output_file_name, output_array)

    extension = '.txt'

    output_file_name = basename + '_ToF-E_values' + extension
    output_array = np.vstack((time_differences,
                              coincidence_energies_a_with_repetitions,
                              coincidence_energies_b)).T

    print("Writing ToF values to: {}".format(output_file_name))
    np.savetxt(output_file_name, output_array)

else:
    #plt.ion()

    fig = plt.figure()
    fig.suptitle("Time of Flight histogram")

    histo_t_ax = fig.add_subplot(111)

    histo_t_ax.step(ToF_edges[:-1], ToF_histo)
    histo_t_ax.set_xlabel('Time [ns]')
    histo_t_ax.grid()

    fig = plt.figure()
    fig.subplots_adjust(hspace = 0.5)
    fig.suptitle("Energy spectra")

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

    if args.save_plots:
    	output_file_name = basename + '_E-histos.' + args.images_extension

    	print("Saving plot to: {}".format(output_file_name))
    	fig.savefig(output_file_name)

    fig = plt.figure(figsize = (fig_width, fig_height / 2 * 3))
    fig.subplots_adjust(bottom = 0.08, top = 0.92, hspace = 0.3)
    fig.suptitle("Energy vs Time of Flight spectra")

    histo_t_ax = fig.add_subplot(311)

    histo_t_ax.step(ToF_edges[:-1], ToF_histo)
    histo_t_ax.set_ylabel('Counts')
    #histo_t_ax.set_xlabel('Time [ns]')
    histo_t_ax.set_title('Time of Flight histogram')
    histo_t_ax.grid()

    bihistos_ax_b = fig.add_subplot(312, sharex = histo_t_ax)

    cax = bihistos_ax_b.imshow(EvsToF_histo_b,
                               origin = 'lower',
                               norm = LogNorm(),
                               interpolation = 'none',
                               extent = (time_min, time_max, energy_min, energy_max),
                               aspect = 'auto')
    #cbar = fig.colorbar(cax)

    #bihistos_ax_b.set_xlabel('Time [ns]')
    bihistos_ax_b.set_ylabel('Energy [ch]')
    bihistos_ax_b.grid()
    bihistos_ax_b.set_title("Channel: {}".format(channel_b))


    bihistos_ax_a = fig.add_subplot(313, sharex = histo_t_ax)

    cax = bihistos_ax_a.imshow(EvsToF_histo_a,
                               origin = 'lower',
                               norm = LogNorm(),
                               interpolation = 'none',
                               extent = (time_min, time_max, reference_energy_min, reference_energy_max),
                               aspect = 'auto')
    #cbar = fig.colorbar(cax)

    bihistos_ax_a.set_xlabel('Time [ns]')
    bihistos_ax_a.set_ylabel('Energy [ch]')
    bihistos_ax_a.grid()
    bihistos_ax_a.set_title("Reference channel: {}".format(channel_a))
    if args.save_plots:
    	output_file_name = basename + '_E-ToF-histos.' + args.images_extension

    	print("Saving plot to: {}".format(output_file_name))
    	fig.savefig(output_file_name)

    fig = plt.figure(figsize = (fig_height, fig_height))
    fig.subplots_adjust(left = 0.18)
    fig.suptitle("Energy of ch {} vs energy of ch {}".format(channel_a, channel_b))

    bihisto_ax = fig.add_subplot(111)

    cax = bihisto_ax.imshow(EvsE_histo,
                            origin = 'lower',
                            norm = LogNorm(),
                            interpolation = 'none',
                            extent = (reference_energy_min, reference_energy_max, energy_min, energy_max),
                            aspect = 'auto')
    #cbar = fig.colorbar(cax)

    bihisto_ax.set_ylabel('Energy (reference channel {}) [ch]'.format(channel_a))
    bihisto_ax.set_xlabel('Energy (channel {}) [ch]'.format(channel_b))
    bihisto_ax.grid()

    plt.show()
