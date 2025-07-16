#! /usr/bin/env python3

#  (C) Copyright 2025, European Union, Cristiano Lino Fontana
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
import traceback

import numpy as np
import matplotlib.pyplot as plt
import scipy.optimize as optimize

DELTA_BINS = 1000

# This should be 160 MB
BUFFER_SIZE = 16 * 10 * 1024 * 1024

PLOT_MARKER = '.'

def exponential(t, A, tau):
    return A * np.exp(-t / tau)

parser = argparse.ArgumentParser(description='Plots subsequent timestamps differences from ABCD events data files.')
parser.add_argument('file_name',
                    type = str,
                    help = 'File name')
parser.add_argument('-n',
                    '--ns_per_sample',
                    type = float,
                    default = None,
                    help = 'Nanoseconds per sample, if specified the timestamps are converted')
parser.add_argument('-N',
                    '--delta_bins',
                    type = int,
                    default = DELTA_BINS,
                    help = 'Number of bins in the time differences histogram (default: {:d})'.format(DELTA_BINS))
parser.add_argument('-d',
                    '--delta_min',
                    type = float,
                    default = None,
                    help = 'Minimum time difference, if not specified the absolute maximum is used')
parser.add_argument('-D',
                    '--delta_max',
                    type = float,
                    default = None,
                    help = 'Maximum time difference, if not specified the absolute maximum is used')
parser.add_argument('-t',
                    '--timestamp_min',
                    type = float,
                    default = 0,
                    help = 'Minimum value accepted for the timestamps, in seconds or timestamp unit (default: 0)')
parser.add_argument('-T',
                    '--timestamp_max',
                    type = float,
                    default = None,
                    help = 'Maximum value to the timestamps, if not specified the absolute maximum is used')
parser.add_argument('-B',
                    '--buffer_size',
                    type = int,
                    default = BUFFER_SIZE,
                    help = 'Buffer size for file reading (default: {:f})'.format(BUFFER_SIZE))
parser.add_argument('-s',
                    '--save_data',
                    action = "store_true",
                    help = 'Save histograms to file')

args = parser.parse_args()

if args.ns_per_sample is None:
    conversion_factor = 1.0
    unit = 'ch'
else:
    conversion_factor = args.ns_per_sample * 1e-9
    unit = 's'

event_PSD_dtype = np.dtype([('timestamp', np.uint64),
                            ('qshort', np.uint16),
                            ('qlong', np.uint16),
                            ('baseline', np.uint16),
                            ('channel', np.uint8),
                            ('pur', np.uint8),
                            ])

buffer_size = args.buffer_size - (args.buffer_size % 16)
print("Using buffer size: {:d}".format(buffer_size))

file_name = args.file_name
print("Reading: {}".format(file_name))

time_deltas = list()

index_previous = 0

events_counter = 0
chunks_counter = 0

# We will read the file in chunks so we can process also very big files
with open(file_name, "rb") as input_file:
    while True:
        try:
            print("    Reading chunk: {:d}".format(chunks_counter))

            file_chunk = input_file.read(buffer_size)

            print("    Chunk length: {:d}".format(len(file_chunk)))

            if len(file_chunk) <= 0:
                break

            data = np.frombuffer(file_chunk, dtype = event_PSD_dtype)

            this_timestamps = data['timestamp']

            timestamp_selection = (args.timestamp_min / conversion_factor) <= this_timestamps

            if args.timestamp_max is not None:
                timestamp_selection = np.logical_and(timestamp_selection, this_timestamps < (args.timestamp_max /  conversion_factor))

            events_counter += timestamp_selection.sum()

            timestamps_left = np.asarray(this_timestamps[timestamp_selection][:-1], dtype=np.float64)
            timestamps_right = np.asarray(this_timestamps[timestamp_selection][1:], dtype=np.float64)

            partial_time_deltas = (timestamps_right - timestamps_left) * conversion_factor

            time_deltas.extend(partial_time_deltas)

            chunks_counter += 1

        except Exception as error:
            print("    ERROR: {}".format(error))
            break

standard_deviation = np.std(time_deltas)

if args.delta_min is None:
    delta_min = min(time_deltas)
else:
    delta_min = args.delta_min

if args.delta_max is None:
    delta_max = max(time_deltas)
else:
    delta_max = args.delta_max

print("Time deltas: [{:f}, {:f}]; Standard deviation: {:f}".format(min(time_deltas), max(time_deltas), standard_deviation))

time_deltas_histo, time_deltas_edges = np.histogram(time_deltas,
                                                    bins = args.delta_bins,
                                                    range = (delta_min, delta_max))

if args.save_data:
    try:
        basename, extension = os.path.splitext(file_name)

        output_file_name = basename + '_ch{}_timedelta.csv'.format(channel)
        output_array = np.vstack((time_deltas_edges[:-1], time_deltas_histo)).T

        print("    Writing qlong histogram to: {}".format(output_file_name))
        np.savetxt(output_file_name, output_array, delimiter = ',', header = 'energy,counts')
    except Exception as error:
        print("    ERROR: {}".format(error))
        print(traceback.format_exc())
else:
    fig = plt.figure()

    td_ax = fig.add_subplot(111)

    td_ax.step(time_deltas_edges[:-1],
               time_deltas_histo,
               where = 'post')

    td_ax.set_xlabel('$\\delta t = t_{n} - t_{n-1}$' + ' [{}]'.format(unit))
    td_ax.grid()
    #td_ax.legend()

    plt.show()
