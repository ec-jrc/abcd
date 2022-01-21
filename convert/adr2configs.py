#! /usr/bin/env python3

# (C) Copyright 2021, European Union, Cristiano Lino Fontana
#
# This file is part of ABCD.
#
# ABCD is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# ABCD is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with ABCD.  If not, see <http://www.gnu.org/licenses/>.

import argparse
from hashlib import new
import logging
import os
import gzip
import lzma
import bz2
import json

parser = argparse.ArgumentParser(description='Read an ABCD raw file extracting the configuration files contained in it')
parser.add_argument('file_name',
                    type = str,
                    help = 'Input file name')
parser.add_argument('output_name',
                    type = str,
                    help = 'Basename of the output files')
parser.add_argument('-v',
                    '--verbose',
                    action = "store_true",
                    help = 'Set verbose execution')

args = parser.parse_args()

if args.verbose:
    logging.basicConfig(level=logging.DEBUG)
else:
    logging.basicConfig(level=logging.INFO)

inputfile_basename, inputfile_extension = os.path.splitext(args.file_name)
outputfile_basename, outputfile_extension = os.path.splitext(args.output_name)

logging.debug("input file basename: {}; file extension: {}".format(inputfile_basename, inputfile_extension))
logging.debug("output file basename: {}; file extension: {}".format(outputfile_basename, outputfile_extension))

file_name_config = outputfile_basename + r'_config_{}_{:d}_{}.json'

if inputfile_extension == ".gz":
    logging.debug("Using gzip file opener")
    file_open = gzip.open
elif inputfile_extension == ".bz2":
    logging.debug("Using bz2 file opener")
    file_open = bz2.open
elif inputfile_extension == ".xz":
    logging.debug("Using lzma file opener")
    file_open = lzma.open
elif inputfile_extension == ".adr":
    logging.debug("Using standard file opener")
    file_open = open
else:
    logging.warning("File extension not recognized, treating the file as regular 'adr' file.")
    file_open = open

with file_open(args.file_name, "rb") as input_file:
    counter_bytes = 0
    counter_topics = 0
    counters_packets_status = dict()
    counters_configs = dict()
    last_configs = dict()

    for module in ["abcd", "waan"]:
        counters_packets_status[module] = 0
        counters_configs[module] = 0
        last_configs[module] = dict()

    byte = input_file.read(1)

    topic_buffer = b""

    while byte != b"":
        counter_bytes += 1

        #logging.debug("topic_buffer: {}".format(topic_buffer))

        if byte != b" ":
            topic_buffer += byte
        else:
            topic = topic_buffer.decode('ascii')

            logging.debug("Topic [{:d}]: {}".format(counter_topics, topic))
            
            counter_topics += 1

            # Looking for the size of the message by searching for the last '_s'
            index = topic.rfind('_s')

            if index == -1 or index >= (len(topic) - 2):
                logging.error("Unable to find message size, topic: {}".format(topic))
            else:
                message_size = int(topic[index + 2:])
                logging.debug("Message size: {:d}".format(message_size))

                message_buffer = input_file.read(message_size)
                counter_bytes += message_size

                if len(message_buffer) != message_size:
                    logging.error("Unable to read all the requested bytes: read: {:d}, requested: {:d}".format(len(message_buffer), message_size))

                for module in ["abcd", "waan"]:
                    topic_begin = "status_{}".format(module)
                    compared_status = topic.find(topic_begin, 0, len(topic_begin))

                    logging.debug("compared_status: {:d}".format(compared_status))

                    if compared_status == 0:
                        counters_packets_status[module] += 1

                        new_status = json.loads(message_buffer)
                        new_config = new_status["config"]
                        timestamp = new_status["timestamp"]

                        if new_config != last_configs[module]:
                            logging.info("Found {} config, counter: {:d}".format(module, counters_configs[module]))

                            with open(file_name_config.format(module, counters_configs[module], timestamp), "w") as output_file:
                                json.dump(new_config, output_file, indent = 4)

                            counters_configs[module] += 1
                            last_configs[module] = new_config

                    logging.debug("packets: {:d}; {} status packets: {:d}".format(counter_topics, module, counters_packets_status[module]))

            topic_buffer = b""

        byte = input_file.read(1)
