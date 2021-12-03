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
                    help = 'Minimum time difference (default: {:f})'.format(DELTA_MIN))
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

timestamps = dict()
time_deltas_histos = dict()
time_deltas_edges = dict()
As = dict()
taus = dict()

buffer_size = args.buffer_size - (args.buffer_size % 16)
print("Using buffer size: {:d}".format(buffer_size))

for file_name in args.file_names:
    print("Reading: {}".format(file_name))

    partial_timestamps = list()
    min_times = list()
    max_times = list()
    counter = 0
    events_counter = 0

    # We will read the file in chunks so we can process also very big files
    with open(file_name, "rb") as input_file:
        while True:
            try:
                print("    Reading chunk: {:d}".format(counter))

                file_chunk = input_file.read(buffer_size)

                data = np.frombuffer(file_chunk, dtype = event_PSD_dtype)

                this_timestamps = data['timestamp']

                min_times.append(min(this_timestamps))
                max_times.append(max(this_timestamps))

                try:
                    channel = int(args.channel)
                    channel_selection = data['channel'] == channel
                except ValueError:
                    channel_selection = data['channel'] >= 0

                events_counter += channel_selection.sum()

                partial_timestamps.extend(this_timestamps[channel_selection])

                counter += 1

            except Exception as error:
                print("    ERROR: {}".format(error))
                break

    try:
        min_time = min(min_times) * conversion_factor
        max_time = max(max_times) * conversion_factor

        Delta_time = max_time - min_time

        rate_measured = events_counter / Delta_time

        print("    Number of events: {:d}".format(events_counter))
        print("    Time interval: [{:g}, {:g}] {}".format(min_time, max_time, unit))
        print("    Time delta: {:g} {}".format(Delta_time, unit))
        print("    Measured rate: {:g} 1/{}".format(rate_measured, unit))

        timestamps[file_name] = partial_timestamps

        timestamps_left = np.asarray(partial_timestamps[:-1]) * conversion_factor
        timestamps_right = np.asarray(partial_timestamps[1:]) * conversion_factor

        time_deltas = timestamps_right - timestamps_left

        if args.delta_max is None:
            delta_max = max(time_deltas)
        else:
            delta_max = args.delta_max

        histo, edges = np.histogram(time_deltas,
                                    bins = args.delta_bins,
                                    range = (args.delta_min, delta_max))

        time_deltas_histos[file_name] = histo
        time_deltas_edges[file_name] = edges

        A = histo.max()
        I = np.trapz(histo, x = edges[:-1])
        tau = I / A

        p0 = (A, tau)

        popt, pcov = optimize.curve_fit(exponential, edges[:-1], histo, p0=p0)

        As[file_name] = popt[0]
        taus[file_name] = popt[1]
        
        rate_true = 1. / taus[file_name]

        dead_time = (rate_true - rate_measured) / (rate_true * rate_measured)

        print("    True rate: {:g} 1/{}".format(rate_true, unit))
        print("    Dead time: {:g} {} ({:f}%)".format(dead_time, unit, dead_time / Delta_time * 100))

    except Exception as error:
        print("    ERROR: {}".format(error))

if args.save_data:
    for file_name in args.file_names:
        try:
            basename, extension = os.path.splitext(file_name)

            output_file_name = basename + '_ch{}_timedelta.csv'.format(channel)
            output_array = np.vstack((time_deltas_edges[file_name][:-1], time_deltas_histos[file_name])).T

            print("    Writing qlong histogram to: {}".format(output_file_name))
            np.savetxt(output_file_name, output_array, delimiter = ',', header = 'energy,counts')
        except Exception as error:
            print("    ERROR: {}".format(error))
else:
    fig = plt.figure()

    ts_ax = fig.add_subplot(111)

    for file_name in args.file_names:
        try:
            ts_ax.plot(np.asarray(timestamps[file_name]) * conversion_factor,
                       label = file_name)
        except Exception as error:
            print("    ERROR: {}".format(error))

    ts_ax.set_ylabel('Timestamps [{}]'.format(unit))
    ts_ax.set_xlabel('Timestamp index')
    ts_ax.grid()
    ts_ax.legend()

    fig = plt.figure()

    td_ax = fig.add_subplot(111)

    for file_name in args.file_names:
        try:
            td_ax.step(time_deltas_edges[file_name][:-1],
                       time_deltas_histos[file_name],
                       where = 'post', label = file_name)

            td_ax.plot(time_deltas_edges[file_name][:-1],
                       exponential(time_deltas_edges[file_name][:-1], As[file_name], taus[file_name]),
                       label = 'Fit')
        except Exception as error:
            print("    ERROR: {}".format(error))

    td_ax.set_xlabel('$\delta t = t_{n} - t_{n-1}$' + ' [{}]'.format(unit))
    td_ax.grid()
    td_ax.legend()

    plt.show()
