#! /usr/bin/env python3

#  (C) Copyright 2016, 2019, 2022, European Union, Cristiano Lino Fontana
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

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.colors import LogNorm

NS_PER_SAMPLE = 2.0 / 1024
TIME_RESOLUTION = 2.0
TIME_MIN = -1
TIME_MAX = -1
ENERGY_RESOLUTION = 20
ENERGY_MIN = 0
ENERGY_MAX = 66000
EVENTS_COUNT = -1
IMAGES_EXTENSION = 'pdf'

parser = argparse.ArgumentParser(description='Plot the time dependency of the energy spectrum from ABCD events data files.')
parser.add_argument('file_name',
                    type = str,
                    help = 'Input file name')
parser.add_argument('channel',
                    type = str,
                    help = 'Channel selection (all or number)')
parser.add_argument('-n',
                    '--ns_per_sample',
                    type = float,
                    default = NS_PER_SAMPLE,
                    help = 'Nanoseconds per sample (default: {:f})'.format(NS_PER_SAMPLE))
parser.add_argument('-N',
                    '--events_count',
                    type = int,
                    default = EVENTS_COUNT,
                    help = 'Maximum number of events to be read from file, if -1 then read all events (default: {:f})'.format(EVENTS_COUNT))
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
parser.add_argument('--save_plots',
                    action = "store_true",
                    help = 'Save plots to file')
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

print("### ### Reading: {}".format(args.file_name))
data = np.fromfile(args.file_name, dtype = event_PSD_dtype, count = args.events_count)

try:
    channel = int(args.channel)
    channel_selection = data['channel'] == channel

except ValueError:
    channel_selection = data['channel'] >= 0

basename, extension = os.path.splitext(args.file_name)

basename += '_Ch{}'.format(args.channel)


qlongs = data['qlong'][channel_selection]
timestamps = data['timestamp'][channel_selection] * 1e-9 * args.ns_per_sample

if args.time_min > 0:
    time_min = args.time_min
else:
    time_min = min(timestamps)

if args.time_max > 0:
    time_max = args.time_max
else:
    time_max = max(timestamps)

Delta_time = time_max - time_min
time_resolution = args.time_resolution
N_t = math.floor((Delta_time) / time_resolution)

energy_min = args.energy_min
energy_max = args.energy_max
energy_resolution = args.energy_resolution
N_E = math.floor((energy_max - energy_min) / energy_resolution)
Delta_bins_E = (energy_max - energy_min) / N_E

max_bin_edge_energy = energy_min + energy_resolution * N_E
max_bin_edge_time = time_min + time_resolution * N_t
Delta_bins_time = time_resolution * N_t


print("Energy min: {:f} ch".format(energy_min))
print("Energy max: {:f} ch".format(energy_max))
print("N_E: {:d}".format(N_E))

print("Time min: {:f} s".format(time_min))
print("Time max: {:f} s".format(time_max))
print("Time delta: {:f} s".format(Delta_time))
print("N_t: {:d}".format(N_t))

histo2d, energy_edges, time_edges = np.histogram2d(qlongs, timestamps, bins = (N_E, N_t), range = ((energy_min, max_bin_edge_energy), (time_min, max_bin_edge_time)))

mean_rates, time_edges = np.histogram(timestamps, bins = time_edges)
mean_rates = mean_rates / time_resolution / 1000


fig = plt.figure()

bihisto_ax = fig.add_subplot(111)

bihisto_ax.imshow(histo2d,
      origin = 'lower',
      norm = LogNorm(),
      interpolation = 'none',
      extent = (min(time_edges), max(time_edges),
                min(energy_edges), max(energy_edges)),
      aspect = 'auto')

bihisto_ax.set_ylabel('Energy [ch]')
bihisto_ax.set_xlabel('Time [s]')

bihisto_ax.grid()

if args.save_plots:
    output_file_name = basename + '_Evst.' + args.images_extension

    print("Saving plot to: {}".format(output_file_name))
    fig.savefig(output_file_name)

fig = plt.figure()

rate_ax = fig.add_subplot(111, sharex = bihisto_ax)

rate_ax.step(time_edges[:-1], mean_rates)
rate_ax.set_xlim(time_min, max_bin_edge_time)
rate_ax.set_ylabel("Rate [kHz]")
rate_ax.set_xlabel('Time [s]')
rate_ax.grid()

if args.save_plots:
    output_file_name = basename + '_Rvst.' + args.images_extension

    print("Saving plot to: {}".format(output_file_name))
    fig.savefig(output_file_name)

plt.show()
