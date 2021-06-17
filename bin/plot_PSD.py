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
from matplotlib.colors import LogNorm

NS_PER_SAMPLE = 2.0 / 1024
PSD_RESOLUTION = 0.01
PSD_MIN = -0.1
PSD_MAX = 0.7
PSD_THRESHOLD = 0.17
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
parser.add_argument('-t',
                    '--PSD_threshold',
                    type = float,
                    default = None,
                    help = 'Simple PSD threshold for n/γ discrimination (default: {:f})'.format(PSD_THRESHOLD))
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
parser.add_argument('-r',
                    '--PSD_resolution',
                    type = float,
                    default = PSD_RESOLUTION,
                    help = 'PSD resolution (default: {:f})'.format(PSD_RESOLUTION))
parser.add_argument('-p',
                    '--PSD_min',
                    type = float,
                    default = PSD_MIN,
                    help = 'PSD min (default: {:f})'.format(PSD_MIN))
parser.add_argument('-P',
                    '--PSD_max',
                    type = float,
                    default = PSD_MAX,
                    help = 'PSD max (default: {:f})'.format(PSD_MAX))
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
parser.add_argument('-s',
                    '--save_data',
                    action = "store_true",
                    help = 'Save histograms to file')
parser.add_argument('--polygon_file',
                    type = str,
                    default = None,
                    help = 'Filename with a polygon to be drawn on top of the plot')

args = parser.parse_args()

event_PSD_dtype = np.dtype([('timestamp', np.uint64),
                            ('qshort', np.uint16),
                            ('qlong', np.uint16),
                            ('baseline', np.uint16),
                            ('channel', np.uint8),
                            ('pur', np.uint8),
                            ])

spectra_energy = list()
spectra_PSD = list()
histos2d = list()

buffer_size = args.buffer_size - (args.buffer_size % 16)
print("Using buffer size: {:d}".format(buffer_size))

min_PSD = args.PSD_min
max_PSD = args.PSD_max
PSD_resolution = args.PSD_resolution
N_PSD = math.floor((max_PSD - min_PSD) / PSD_resolution)
max_bin_edge_PSD = min_PSD + N_PSD * PSD_resolution

min_energy = args.energy_min
max_energy = args.energy_max
energy_resolution = args.energy_resolution
N_E = math.floor((max_energy - min_energy) / energy_resolution)
max_bin_edge_energy = min_energy + N_E * energy_resolution

for file_name in args.file_names:
    print("Reading: {}".format(file_name))

    partial_spectra_energy = list()
    partial_spectra_PSD = list()
    partial_histos2d = list()

    min_times = list()
    max_times = list()

    energy_edges = None

    counter = 0
    events_counter = 0
    above_counter = 0
    below_counter = 0

    # We will read the file in chunks so we can process also very big files
    with open(file_name, "rb") as input_file:
        while True:
            try:
                print("    Reading chunk: {:d}".format(counter))

                file_chunk = input_file.read(buffer_size)

                data = np.frombuffer(file_chunk, dtype = event_PSD_dtype)

                timestamps = data['timestamp']
                qlongs = data['qlong']
                qshorts = data['qshort']
                PSDs = (qlongs.astype(np.float64) - qshorts) / qlongs

                min_times.append(min(timestamps))
                max_times.append(max(timestamps))

                try:
                    channel = int(args.channel)
                    channel_selection = data['channel'] == channel
                except ValueError:
                    channel_selection = data['channel'] >= 0

                events_counter += channel_selection.sum()

                if args.PSD_threshold is not None:
                    above_counter += np.sum(np.logical_and(PSDs >= args.PSD_threshold, channel_selection))
                    below_counter += np.sum(np.logical_and(PSDs < args.PSD_threshold, channel_selection))

                energy_selection = np.logical_and(min_energy < qlongs, qlongs < max_energy)
                PSD_selection = np.logical_and(min_PSD < PSDs, PSDs < max_PSD)

                selection = np.logical_and(channel_selection, np.logical_and(PSD_selection, energy_selection))

                this_spectrum_energy, energy_edges = np.histogram(qlongs[selection],
                                                                  bins = N_E,
                                                                  range = (min_energy, max_bin_edge_energy))
                this_spectrum_PSD, PSD_edges = np.histogram(PSDs[selection],
                                                            bins = N_PSD,
                                                            range = (min_PSD, max_bin_edge_PSD))
                this_histo2d, PSD_edges, energy_edges  = np.histogram2d(PSDs[selection], qlongs[selection],
                                                                        bins = (N_PSD, N_E),
                                                                        range = ((min_PSD, max_bin_edge_PSD), (min_energy, max_bin_edge_energy)))

                partial_spectra_energy.append(this_spectrum_energy)
                partial_spectra_PSD.append(this_spectrum_PSD)
                partial_histos2d.append(this_histo2d)

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

        if args.PSD_threshold is not None:
            print("    Above PSD threshold: {:d} ({:f} %)".format(above_counter, above_counter / events_counter * 100))
            print("    Below PSD threshold: {:d} ({:f} %)".format(below_counter, below_counter / events_counter * 100))

        int_spectrum_energy = functools.reduce(np.add, partial_spectra_energy)
        int_spectrum_PSD = functools.reduce(np.add, partial_spectra_PSD)
        int_histo2d = functools.reduce(np.add, partial_histos2d)

        # Using an integer spectrum we do not have rounding errors in the running mean
        smoothed_spectrum_energy = running_mean(int_spectrum_energy, args.smooth_window)
        smoothed_spectrum_PSD = running_mean(int_spectrum_PSD, args.smooth_window)

        # Time normalization of the spectrum
        spectrum_energy = np.array(smoothed_spectrum_energy, dtype = np.float) / Delta_time
        spectrum_PSD = np.array(smoothed_spectrum_PSD, dtype = np.float) / Delta_time
        histo2d = np.array(int_histo2d, dtype = np.float) / Delta_time

        spectra_energy.append(spectrum_energy)
        spectra_PSD.append(spectrum_PSD)
        histos2d.append(histo2d)

    except Exception as error:
        spectra_energy.append(np.zeros(N_E))
        spectra_PSD.append(np.zeros(N_E))
        histos2d.append(np.zeros((N_PSD, N_E)))
        print("    ERROR: {}".format(error))


histo2d = functools.reduce(np.add, histos2d)

if args.save_data:
    for spectrum_energy, spectrum_PSD, file_name in zip(spectra_energy, spectra_PSD, args.file_names):
        basename, extension = os.path.splitext(file_name)

        output_file_name = basename + '_qlong.csv'
        output_array = np.vstack((energy_edges[:-1], spectrum_energy)).T

        print("    Writing qlong histogram to: {}".format(output_file_name))
        np.savetxt(output_file_name, output_array, delimiter = ',', header = 'energy,counts')

        output_file_name = basename + '_PSD.csv'
        output_array = np.vstack((PSD_edges[:-1], spectrum_PSD)).T

        print("    Writing PSD histogram to: {}".format(output_file_name))
        np.savetxt(output_file_name, output_array, delimiter = ',', header = 'PSD,counts')

else:
    fig = plt.figure()

    spect_energy_ax = None
    spect_PSD_ax = None

    spect_energy_ax = fig.add_subplot(211)
    spect_PSD_ax = fig.add_subplot(212)

    for spectrum_energy, spectrum_PSD, file_name in zip(spectra_energy, spectra_PSD, args.file_names):
        spect_energy_ax.step(energy_edges[:-1], spectrum_energy, label = file_name, where = 'post')
        spect_PSD_ax.step(PSD_edges[:-1], spectrum_PSD, label = file_name, where = 'post')

    spect_energy_ax.set_xlim(min_energy, max_energy)
    spect_energy_ax.set_xlabel('Energy [ch]')
    spect_energy_ax.grid()
    spect_energy_ax.legend()

    spect_PSD_ax.set_xlim(min_PSD, max_PSD)
    spect_PSD_ax.set_xlabel('PSD')
    spect_PSD_ax.grid()
    spect_PSD_ax.legend()

    fig = plt.figure()

    bihisto_ax = fig.add_subplot(111)

    cax = bihisto_ax.imshow(histo2d,
          origin = 'lower',
          norm = LogNorm(),
          interpolation = 'none',
          extent = (min(energy_edges), max(energy_edges),
                    min(PSD_edges), max(PSD_edges)),
          aspect = 'auto')

    cbar = fig.colorbar(cax)

    if args.PSD_threshold is not None:
        bihisto_ax.plot([min(energy_edges), max(energy_edges)], [args.PSD_threshold, args.PSD_threshold], color = 'k')

    #try:
    #    bihisto_ax.plot(polygon_x, polygon_y, color = 'k')
    #except:
    #    pass

    bihisto_ax.set_xlabel('Energy [ch]')
    bihisto_ax.set_ylabel('PSD')
    bihisto_ax.grid()

    plt.show()
