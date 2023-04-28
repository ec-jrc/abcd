#! /usr/bin/env python3

#  (C) Copyright 2021, European Union, Cristiano Lino Fontana
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
DELTA_MIN = 0

# This should be 160 MB
BUFFER_SIZE = 16 * 10 * 1024 * 1024

def exponential(t, A, tau):
    return A * np.exp(-t / tau)

parser = argparse.ArgumentParser(description='Plots multiple timestamps sequences and distributions of time differences from ABCD events data files.')
parser.add_argument('file_name',
                    type = str,
                    help = 'File name')
parser.add_argument('channels',
                    type = int,
                    nargs = '+',
                    help = 'List of space-separated channels to select')
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
                    default = DELTA_MIN,
                    help = 'Minimum time difference, in seconds or timestamp unit (default: {:f})'.format(DELTA_MIN))
parser.add_argument('-D',
                    '--delta_max',
                    type = float,
                    default = None,
                    help = 'Maximum time difference, if not specified the absolute maximum is used')
parser.add_argument('-B',
                    '--buffer_size',
                    type = int,
                    default = BUFFER_SIZE,
                    help = 'Buffer size for file reading (default: {:f})'.format(BUFFER_SIZE))
parser.add_argument('-s',
                    '--save_data',
                    action = "store_true",
                    help = 'Save histograms to file')
parser.add_argument('--relative_indexes',
                    action = "store_true",
                    help = 'Instead of plotting vs the aboslute indexes, use the consecutive indexes of the timestamps')

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

indexes_timestamps = {channel: list() for channel in args.channels}
partial_timestamps = {channel: list() for channel in args.channels}

index_previous = 0

min_times = {channel: list() for channel in args.channels}
max_times = {channel: list() for channel in args.channels}
events_counter = {channel: 0 for channel in args.channels}
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

            indexes = np.arange(index_previous, index_previous + len(data))

            this_timestamps = data['timestamp']

            for channel in args.channels:
                channel_selection = data['channel'] == channel

                events_counter[channel] += channel_selection.sum()

                indexes_timestamps[channel].extend(indexes[channel_selection])
                partial_timestamps[channel].extend(this_timestamps[channel_selection])

                min_times[channel].append(min(partial_timestamps[channel]))
                max_times[channel].append(max(partial_timestamps[channel]))


            index_previous += len(data)

            chunks_counter += 1

        except Exception as error:
            print("    ERROR: {}".format(error))
            break

time_deltas_histos = dict()
time_deltas_edges = dict()
As = dict()
taus = dict()

for channel in args.channels:
    min_time = min(min_times[channel])
    max_time = max(max_times[channel])

    Delta_time = (max_time - min_time) * conversion_factor

    rate_measured = events_counter[channel] / Delta_time

    print("Channel: {:d}".format(channel))
    print("    Number of events: {:d}".format(events_counter[channel]))
    print("    Time interval: [{:g}, {:g}] {}".format(min_time * conversion_factor,
                                                      max_time * conversion_factor, unit))
    print("    Time delta: {:g} {}".format(Delta_time, unit))
    print("    Measured rate: {:g} events/{}".format(rate_measured, unit))

    try:
        timestamps_left = np.asarray(partial_timestamps[channel][:-1])
        timestamps_right = np.asarray(partial_timestamps[channel][1:])

        time_deltas = (timestamps_right - timestamps_left) * conversion_factor

        if args.delta_max is None:
            delta_max = max(time_deltas)
        else:
            delta_max = args.delta_max

        histo, edges = np.histogram(time_deltas,
                                   bins = args.delta_bins,
                                   range = (args.delta_min, delta_max))

        time_deltas_histos[channel] = histo
        time_deltas_edges[channel] = edges

        A = histo.max()
        I = np.trapz(histo, x = edges[:-1])
        tau = I / A

        p0 = (A, tau)

        popt, pcov = optimize.curve_fit(exponential, edges[:-1], histo, p0=p0)

        As[channel] = popt[0]
        taus[channel] = popt[1]

        rate_true = 1. / taus[channel]

        dead_time = Delta_time - (events_counter[channel] / rate_true)

        print("    True rate: {:g} events/{}".format(rate_true, unit))
        print("    Dead time: {:g} {} ({:f}%)".format(dead_time, unit, dead_time / Delta_time * 100))

    except Exception as error:
        print("    ERROR: {}".format(error))
        print(traceback.format_exc())

if args.save_data:
    try:
        basename, extension = os.path.splitext(file_name)

        output_file_name = basename + '_ch{}_timedelta.csv'.format(channel)
        output_array = np.vstack((time_deltas_edges[:-1], time_deltas_histos)).T

        print("    Writing qlong histogram to: {}".format(output_file_name))
        np.savetxt(output_file_name, output_array, delimiter = ',', header = 'energy,counts')
    except Exception as error:
        print("    ERROR: {}".format(error))
        print(traceback.format_exc())
else:
    fig = plt.figure()

    ts_ax = fig.add_subplot(111)

    for channel in args.channels:
        try:
            if args.relative_indexes:
                ts_ax.plot(np.asarray(partial_timestamps[channel]) * conversion_factor,
                           linewidth = 0, marker = ',',
                           label = "Ch: {:d}".format(channel))
            else:
                ts_ax.plot(indexes_timestamps[channel],
                           np.asarray(partial_timestamps[channel]) * conversion_factor,
                           linewidth = 0, marker = ',',
                           label = "Ch: {:d}".format(channel))
        except Exception as error:
            print("    ERROR: {}".format(error))
            print(traceback.format_exc())

    ts_ax.set_ylabel('Timestamps [{}]'.format(unit))
    ts_ax.set_xlabel('Timestamp index')
    ts_ax.grid()
    ts_ax.legend()

    fig = plt.figure()

    td_ax = fig.add_subplot(111)

    for channel in args.channels:
        try:
            td_ax.step(time_deltas_edges[channel][:-1],
                       time_deltas_histos[channel],
                       where = 'post', label = "Ch: {:d}".format(channel))

            td_ax.plot(time_deltas_edges[channel][:-1],
                       exponential(time_deltas_edges[channel][:-1], As[channel], taus[channel]),
                       label = 'Fit')
        except Exception as error:
            print("    ERROR: {}".format(error))
            print(traceback.format_exc())

    td_ax.set_xlabel('$\delta t = t_{n} - t_{n-1}$' + ' [{}]'.format(unit))
    td_ax.grid()
    td_ax.legend()

    plt.show()
