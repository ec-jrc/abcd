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
import sys

parser = argparse.ArgumentParser(description='Read and print an ABCD events file converting it to ASCII')
parser.add_argument('file_name',
                    type = str,
                    help = 'Name of the input file')
parser.add_argument('-o',
                    '--output_name',
                    type = str,
                    default = None,
                    help = 'Write to a file instead of the stdout')

args = parser.parse_args()

# Struct used to read the events binary data.
# An event is a 16 bytes structure composed of:
#  64 bits - timestamp (format string: Q)
#  16 bits - qshort    (format string: H)
#  16 bits - qlong     (format string: H)
#  16 bits - baseline  (format string: H)
#   8 bits - channel   (format string: B)
#   8 bits - PUR flag  (format string: B)
event_PSD_struct = struct.Struct("<QHHHBB")
event_PSD_size = event_PSD_struct.size

output_file = sys.stdout

if args.output_name is not None:
    output_file = open(args.output_name, 'w')

with open(args.file_name, 'rb') as input_file:
    output_file.write("#N\ttimestamp\tqshort\tqlong\tchannel\n")

    counter = 0

    try:
        while True:
            binary_data = input_file.read(event_PSD_size)

            timestamp, qshort, qlong, baseline, channel, pur = event_PSD_struct.unpack(binary_data)

            output_file.write("{:d}\t{:d}\t{:d}\t{:d}\t{:d}\n".format(counter, timestamp, qshort, qlong, channel))
            counter += 1
    except struct.error:
        pass

if args.output_name is not None:
    output_file.close()
