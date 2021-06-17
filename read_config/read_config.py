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
import json
import pprint

parser = argparse.ArgumentParser(description='Read and print config settings from a raw datafile')
parser.add_argument('file_name',
                    type = str,
                    help = 'Input file name')
parser.add_argument('-s',
                    '--save_config',
                    type = str,
                    default = None,
                    help = 'Save config to file')
parser.add_argument('-S',
                    '--skip_packets',
                    type = int,
                    default = 0,
                    help = 'Skip status packets')

args = parser.parse_args()

with open(args.file_name, 'rb') as input_file:
    topic_buffer = list()
    status_packets_counter = 0

    byte = input_file.read(1)
    while byte:
        if byte != b' ':
            topic_buffer.append(byte)
        else:
            topic = "".join([b.decode('ascii') for b in topic_buffer])
            print('Topic: {}'.format(topic))

            index = topic.rfind('_')
            size = int(topic[index + 2:])

            print("Size: {:d}".format(size))

            chunk = input_file.read(size)

            if topic.startswith('status_abcd_v0'):
                json_message = chunk.decode('ascii').strip()
                message = json.loads(json_message)

                status_packets_counter += 1

                pprint.pprint(message["config"], indent = 4)

                if status_packets_counter > args.skip_packets:
                    if args.save_config is not None:
                        pprint.pprint(message["config"], open(args.save_config, 'w'), indent = 4)

                    break

            topic_buffer = list()

        byte = input_file.read(1)
