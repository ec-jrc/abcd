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
import logging
import os
import gzip
import lzma
import bz2
import zmq
import time

BASE_PERIOD = 100
ADDRESS_STATUS = "tcp://*:16180"
ADDRESS_DATA = "tcp://*:16181"

parser = argparse.ArgumentParser(description='Read and print an ABCD raw file converting it to an events file and a waveforms file')
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

file_name_events = outputfile_basename + '_events.ade'
file_name_waveforms = outputfile_basename + '_waveforms.adw' + '.xz'

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

with file_open(args.file_name, "rb") as input_file, \
     open(file_name_events, "wb") as events_file, \
     lzma.open(file_name_waveforms, "wb") as waveforms_file:

    counter_bytes = 0
    counter_topics = 0
    counter_events_packets = 0
    counter_waveforms_packets = 0
    counter_other_packets = 0

    byte = input_file.read(1)

    topic_buffer = b""

    while byte != b"":
        counter_bytes += 1

        logging.debug("topic_buffer: {}".format(topic_buffer))

        if byte != b" ":
            topic_buffer += byte
        else:
            topic = topic_buffer.decode('ascii')

            logging.info("Topic [{:d}]: {}".format(counter_topics, topic))
            
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

                compared_events = topic.find("data_abcd_events", 0, len("data_abcd_events"))
                compared_waveforms = topic.find("data_abcd_waveforms", 0, len("data_abcd_waveforms"))

                logging.debug("compared_events: {:d}; compared_waveforms: {:d}".format(compared_events, compared_waveforms))

                if compared_events == 0:
                    counter_events_packets += 1
                    events_file.write(message_buffer)
                    logging.debug("Writing to file: {}".format(file_name_events))
                elif compared_waveforms == 0:
                    counter_waveforms_packets += 1
                    waveforms_file.write(message_buffer)
                    logging.debug("Writing to file: {}".format(file_name_waveforms))
                else:
                    counter_other_packets += 1

                logging.debug("packets: {:d}; events packets: {:d}; waveforms packets: {:d}; other packets: {:d}".format(counter_topics, counter_events_packets, counter_waveforms_packets, counter_other_packets))

            topic_buffer = b""

        byte = input_file.read(1)
