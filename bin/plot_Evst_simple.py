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
import struct
import os
import json

import math

import numpy as np
from scipy import optimize
import matplotlib.pyplot as plt
from matplotlib.colors import LogNorm

NS_PER_SAMPLE = 2.0 / 1024
TIME_RESOLUTION = 0.2
ENERGY_RESOLUTION = 20
ENERGY_MIN = 400
ENERGY_MAX = 20000
TIME_MIN = -1
TIME_MAX = -1
EVENTS_COUNT = -1

parser = argparse.ArgumentParser(description='Read and print ABCD data files')
parser.add_argument('file_name',
                    type = str,
                    help = 'Input file name')
parser.add_argument('channel',
                    type = str,
                    help = 'Channel selection (all or number)')
parser.add_argument('-l',
                    '--logscale',
                    action = "store_true",
                    help = 'Display spectra in logscale')
parser.add_argument('-n',
                    '--ns_per_sample',
                    type = float,
                    default = NS_PER_SAMPLE,
                    help = 'Nanoseconds per sample (default: {:f})'.format(NS_PER_SAMPLE))
parser.add_argument('-N',
                    '--events_count',
                    type = int,
                    default = EVENTS_COUNT,
                    help = 'Events to be read from file (default: {:f})'.format(EVENTS_COUNT))
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
                    type = str,
                    default = None,
                    help = 'Save histograms to file')

args = parser.parse_args()

# One more H than expected, I guess the compiler is padding the data
event_PSD_struct = struct.Struct("<QHHHBB")

event_PSD_dtype = np.dtype([('timetag', np.uint64),
                            ('qshort', np.uint16),
                            ('qlong', np.uint16),
                            ('baseline', np.uint16),
                            ('channel', np.uint8),
                            ('pur', np.uint8),
                            ])
                            
print("### ### Reading: {}".format(args.file_name))
data = np.fromfile(args.file_name, dtype = event_PSD_dtype, count = args.events_count) #26905012)

try:
    channel = int(args.channel)
    channel_selection = data['channel'] == channel

except ValueError:
    channel_selection = data['channel'] >= 0
    
qlongs = data['qlong'][channel_selection]
timetags = data['timetag'][channel_selection] * 1e-9 * args.ns_per_sample

if args.time_min > 0:
    min_time = args.time_min
else:
    min_time = min(timetags)

if args.time_max > 0:
    max_time = args.time_max
else:
    max_time = max(timetags)

Delta_time = max_time - min_time
time_resolution = args.time_resolution
N_t = math.floor((Delta_time) / time_resolution)

min_energy = args.energy_min
max_energy = args.energy_max
energy_resolution = args.energy_resolution
N_E = math.floor((max_energy - min_energy) / energy_resolution)
Delta_bins_E = (max_energy - min_energy) / N_E

max_bin_edge_energy = min_energy + energy_resolution * N_E
max_bin_edge_time = min_time + time_resolution * N_t
Delta_bins_time = time_resolution * N_t

print("Time: min: {}, max: {}, max_bin_edge: {}, Delta: {}, Delta bins: {}".format(min_time, max_time, max_bin_edge_time, Delta_time, Delta_bins_time))
print("Energy: min: {:f}, max: {:f}".format(min_energy, max_energy))
print("N_t: {:d}, N_E: {:d}".format(N_t, N_E))

histo2d, energy_edges, time_edges = np.histogram2d(qlongs, timetags, bins = (N_E, N_t), range = ((min_energy, max_bin_edge_energy), (min_time, max_bin_edge_time)))

fig = plt.figure()

bihisto_ax = fig.add_subplot(111)

bihisto_ax.imshow(histo2d,
      origin = 'lower',
      norm = LogNorm(),
      interpolation = 'none',
      extent = (min(time_edges), max(time_edges),
                min(energy_edges) / 13000 * 1440, max(energy_edges) / 13000 * 1440),
      aspect = 'auto')

#bihisto_ax.set_yscale('log')
bihisto_ax.set_ylabel('Energy [keV]')
bihisto_ax.set_xlabel('Time [s]')

bihisto_ax.grid()

plt.show()
