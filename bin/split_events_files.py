#! /usr/bin/env python3

#  (C) Copyright 2019 Cristiano Lino Fontana
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

TIME_MIN = -1
TIME_MAX = -1
TIME_CHUNKS = 1
NS_PER_SAMPLE = 2.0 / 1024

parser = argparse.ArgumentParser(description='Splits events files on the basis of the timestamps')
parser.add_argument('file_name',
                    type = str,
                    help = 'Input file name')
parser.add_argument('-n',
                    '--ns_per_sample',
                    type = float,
                    default = NS_PER_SAMPLE,
                    help = 'Nanoseconds per sample (default: {:f})'.format(NS_PER_SAMPLE))
parser.add_argument('-N',
                    '--time_chunks',
                    type = int,
                    default = TIME_CHUNKS,
                    help = 'Number of chunks to divide the selected time portion (default: {:d})'.format(TIME_CHUNKS))
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
parser.add_argument('-p',
                    '--prefix',
                    type = str,
                    default = None,
                    help = 'Output files prefix (default: the original file name with "_chunk")')

args = parser.parse_args()

event_PSD_dtype = np.dtype([('timestamp', np.uint64),
                            ('qshort', np.uint16),
                            ('qlong', np.uint16),
                            ('baseline', np.uint16),
                            ('channel', np.uint8),
                            ('pur', np.uint8),
                            ])
                            
if args.prefix is not None:
    output_prefix = args.prefix
else:
    basename, extension = os.path.splitext(args.file_name)
    output_prefix = basename + '_chunk'

digits = math.ceil(math.log(args.time_chunks, 10))

number_format = '{:0' + "{:d}".format(digits) + "}"

print("Reading: {}".format(args.file_name))
data = np.fromfile(args.file_name, dtype = event_PSD_dtype)

timestamps = data['timestamp'] * 1e-9 * args.ns_per_sample

if args.time_min > 0:
    min_time = args.time_min
else:
    min_time = min(timestamps)

if args.time_max > 0:
    max_time = args.time_max
else:
    max_time = max(timestamps)

Delta_time = max_time - min_time
time_chunk = Delta_time / args.time_chunks

for i in range(args.time_chunks):
    new_file_name = output_prefix + number_format.format(i) + '.ade'

    this_min_time = min_time + time_chunk * i
    this_max_time = min_time + time_chunk * (i + 1)

    selection = np.logical_and(this_min_time < timestamps, timestamps < this_max_time)

    print("Writing to: {}".format(new_file_name))
    data[selection].tofile(new_file_name, sep = "")
