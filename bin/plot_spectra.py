#! /usr/bin/env python3

#  (C) Copyright 2016, 2019 Cristiano Lino Fontana
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

import math
import functools

import numpy as np
import matplotlib.pyplot as plt

NS_PER_SAMPLE = 2.0 / 1024
ENERGY_RESOLUTION = 20
ENERGY_MIN = 0
ENERGY_MAX = 20000
SMOOTH_WINDOW = 1
# This should be 160 MB
BUFFER_SIZE = 16 * 10 * 1024 * 1024

def running_mean(x, N):
    Np = (N // 2) * 2 + 1

    padded_samples = np.insert(x, 0, np.zeros((N // 2 + 1, )) + x[0])
    padded_samples = np.append(padded_samples, np.zeros((N // 2, )) + x[-1])

    cumsum = np.cumsum(padded_samples)

    return (cumsum[Np:] - cumsum[:-Np]) / Np


parser = argparse.ArgumentParser(description='Plots multiple time normalized spectra from ABCD events data files.')
parser.add_argument('file_names',
                    type = str,
                    nargs = '+',
                    help = 'List of space-separated file names')
parser.add_argument('channel',
                    type = str,
                    help = 'Channel selection (all or number)')
parser.add_argument('-n',
                    '--ns_per_sample',
                    type = float,
                    default = NS_PER_SAMPLE,
                    help = 'Nanoseconds per sample (default: {:f})'.format(NS_PER_SAMPLE))
parser.add_argument('-w',
                    '--smooth_window',
                    type = int,
                    default = SMOOTH_WINDOW,
                    help = 'Smooth window (default: {:f})'.format(SMOOTH_WINDOW))
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
parser.add_argument('-B',
                    '--buffer_size',
                    type = int,
                    default = BUFFER_SIZE,
                    help = 'Buffer size for file reading (default: {:f})'.format(BUFFER_SIZE))
parser.add_argument('-d',
                    '--enable_derivatives',
                    action = "store_true",
                    help = 'Enable spectra derivatives calculation')
parser.add_argument('-s',
                    '--save_data',
                    action = "store_true",
                    help = 'Save histograms to file')

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

min_energy = args.energy_min
max_energy = args.energy_max
energy_resolution = args.energy_resolution
N_E = math.floor((max_energy - min_energy) / energy_resolution)
max_bin_edge_energy = min_energy + N_E * energy_resolution

for file_name in args.file_names:
    print("Reading: {}".format(file_name))

    partial_spectra = list()
    min_times = list()
    max_times = list()
    energy_edges = None
    counter = 0
    events_counter = 0

    # We will read the file in chunks so we can process also very big files
    with open(file_name, "rb") as input_file:
        while True:
            try:
                print("    Reading chunk: {:d}".format(counter))

                file_chunk = input_file.read(buffer_size)

                data = np.frombuffer(file_chunk, dtype = event_PSD_dtype)

                timestamps = data['timestamp']
                qlongs = data['qlong']

                min_times.append(min(timestamps))
                max_times.append(max(timestamps))

                try:
                    channel = int(args.channel)
                    channel_selection = data['channel'] == channel
                except ValueError:
                    channel_selection = data['channel'] >= 0

                events_counter += channel_selection.sum()

                energy_selection = np.logical_and(min_energy < qlongs, qlongs < max_energy)

                selection = np.logical_and(channel_selection, energy_selection)

                this_spectrum, energy_edges = np.histogram(qlongs[selection],
                                                           bins = N_E,
                                                           range = (min_energy, max_bin_edge_energy))

                partial_spectra.append(this_spectrum)

                counter += 1

            except Exception as error:
                print("    ERROR: {}".format(error))
                break

    try:
        min_time = min(min_times)
        max_time = max(max_times)

        Delta_time = (max_time - min_time) * args.ns_per_sample * 1e-9

        print("    Number of events: {:d}".format(events_counter))
        print("    Time delta: {:f} s".format(Delta_time))
        print("    Average rate: {:f} Hz".format(events_counter / Delta_time))

        int_spectrum = functools.reduce(np.add, partial_spectra)

        # Using an integer spectrum we do not have rounding errors in the running mean
        smoothed_spectrum = running_mean(int_spectrum, args.smooth_window)

        # Time normalization of the spectrum
        spectrum = np.array(smoothed_spectrum, dtype = np.float) / Delta_time

        spectrum_derivative = -1 * np.gradient(smoothed_spectrum, energy_resolution)

        spectra.append(spectrum)
        spectra_derivatives.append(spectrum_derivative)

    except Exception as error:
        spectra.append(np.zeros(N_E))
        spectra_derivatives.append(np.zeros(N_E))
        print("    ERROR: {}".format(error))

if args.save_data:
    for spectrum, spectrum_derivative, file_name in zip(spectra, spectra_derivatives, args.file_names):
        basename, extension = os.path.splitext(file_name)

        output_file_name = basename + '_ch{}_qlong.csv'.format(channel)
        output_array = np.vstack((energy_edges[:-1], spectrum)).T

        print("    Writing qlong histogram to: {}".format(output_file_name))
        np.savetxt(output_file_name, output_array, delimiter = ',', header = 'energy,counts')

        if args.enable_derivatives:
            output_file_name = basename + '_deriv.csv'
            output_array = np.vstack((energy_edges[:-1], spectrum_derivative)).T

            print("    Writing derivative of qlong histogram to: {}".format(output_file_name))
            np.savetxt(output_file_name, output_array, delimiter = ',', header = 'energy,counts')
else:
    fig = plt.figure()

    spect_ax = None
    deriv_ax = None

    if args.enable_derivatives:
        spect_ax = fig.add_subplot(211)
        deriv_ax = fig.add_subplot(212, sharex = spect_ax)
    else:
        spect_ax = fig.add_subplot(111)

    for spectrum, file_name in zip(spectra, args.file_names):
        spect_ax.step(energy_edges[:-1], spectrum, label = file_name, where = 'post')

    if args.enable_derivatives:
        for spectrum_derivative, file_name in zip(spectra_derivatives, args.file_names):
            deriv_ax.step(energy_edges[:-1], spectrum_derivative, label = file_name, where = 'post')

        deriv_ax.set_xlim(min_energy, max_energy)
        deriv_ax.set_xlabel('Energy [ch]')
        deriv_ax.grid()
        deriv_ax.legend()

    spect_ax.set_xlim(min_energy, max_energy)
    spect_ax.set_xlabel('Energy [ch]')
    spect_ax.grid()
    spect_ax.legend()

    plt.show()
