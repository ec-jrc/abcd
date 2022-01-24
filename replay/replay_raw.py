#! /bin/env python3
#
# (C) Copyright 2021, EC Commission, JRC, Cristiano Lino Fontana
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
ADDRESS_ABCD_STATUS = "tcp://*:16180"
ADDRESS_ABCD_DATA = "tcp://*:16181"
ADDRESS_WAAN_STATUS = "tcp://*:16206"

parser = argparse.ArgumentParser(description='System simulator that uses ABCD raw files, supporting also compressed files.')
parser.add_argument('file_name',
                    type = str,
                    help = 'Input file name')
parser.add_argument('-T',
                    '--base_period',
                    type = float,
                    default = BASE_PERIOD,
                    help = 'Set base period in milliseconds (default: {:f} ms)'.format(BASE_PERIOD))
parser.add_argument('-S',
                    '--abcd_status_address',
                    type = str,
                    default = ADDRESS_ABCD_STATUS,
                    help = 'abcd status socket address (default: {})'.format(ADDRESS_ABCD_STATUS))
parser.add_argument('-D',
                    '--abcd_data_address',
                    type = str,
                    default = ADDRESS_ABCD_DATA,
                    help = 'Data socket address (default: {})'.format(ADDRESS_ABCD_DATA))
parser.add_argument('-W',
                    '--waan_status_address',
                    type = str,
                    default = ADDRESS_WAAN_STATUS,
                    help = 'waan status socket address (default: {})'.format(ADDRESS_WAAN_STATUS))
parser.add_argument('-w',
                    '--enable_waan_status',
                    action = "store_true",
                    help = 'Enable the publication of waan status messages')
parser.add_argument('-s',
                    '--skip_packets',
                    type = int,
                    default = 0,
                    help = 'Skip initial packets (default: 0)')
parser.add_argument('-v',
                    '--verbose',
                    action = "store_true",
                    help = 'Set verbose execution')
parser.add_argument('-c',
                    '--continuous_execution',
                    action = "store_true",
                    help = 'Enable continuous execution')

args = parser.parse_args()

if args.verbose:
    logging.basicConfig(level=logging.DEBUG)
else:
    logging.basicConfig(level=logging.INFO)

file_basename, file_extension = os.path.splitext(args.file_name)

logging.debug("file basename: {}; file extension: {}".format(file_basename, file_extension))

if file_extension == ".gz":
    logging.debug("Using gzip file opener")
    file_open = gzip.open
elif file_extension == ".bz2":
    logging.debug("Using bz2 file opener")
    file_open = bz2.open
elif file_extension == ".xz":
    logging.debug("Using lzma file opener")
    file_open = lzma.open
elif file_extension == ".adr":
    logging.debug("Using standard file opener")
    file_open = open
else:
    logging.warning("File extension not recognized, treating the file as regular 'adr' file.")
    file_open = open

with zmq.Context() as context:
    socket_abcd_status = context.socket(zmq.PUB)
    socket_abcd_data = context.socket(zmq.PUB)

    socket_abcd_status.bind(args.abcd_status_address)
    socket_abcd_data.bind(args.abcd_data_address)

    if args.enable_waan_status:
        socket_waan_status = context.socket(zmq.PUB)
        socket_waan_status.bind(args.waan_status_address)

    # Wait a bit to prevent the slow-joiner syndrome
    time.sleep(0.3)

    continue_execution = True
    counter_loop = 0

    try:
        while continue_execution:
            logging.debug("Loop number: {:d}".format(counter_loop))
            counter_loop += 1
    
            if not args.continuous_execution:
                continue_execution = False
    
            with file_open(args.file_name, "rb") as input_file:
                counter_bytes = 0
                counter_topics = 0
                counter_packets_abcd_status = 0
                counter_packets_abcd_data = 0
                counter_packets_waan_status = 0
                counter_packets_zipped = 0
                counter_packets_unknown = 0
        
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
    
                            compared_abcd_status = topic.find("status_abcd", 0, len("status_abcd"))
                            compared_abcd_events = topic.find("events_abcd", 0, len("events_abcd"))
                            compared_abcd_data = topic.find("data_abcd", 0, len("data_abcd"))
                            compared_waan_status = topic.find("status_waan", 0, len("status_waan"))
                            compared_waan_events = topic.find("events_waan", 0, len("events_waan"))
                            compared_zipped = topic.find("compressed", 0, len("compressed"))
    
                            logging.debug("compared_abcd_status: {:d}; compared_abcd_events: {:d}; compared_abcd_data: {:d}; compared_zipped: {:d}".format(compared_abcd_status, compared_abcd_events, compared_abcd_data, compared_zipped))
    
                            # If it is a status-like packet send it through the status socket...
                            if (compared_abcd_status == 0 or compared_abcd_events == 0) and \
                                compared_waan_status != 0 and compared_waan_events != 0 and \
                                compared_abcd_data != 0 and compared_zipped != 0:

                                counter_packets_abcd_status += 1
    
                                if counter_topics < args.skip_packets:
                                    logging.debug("Skipping packet")
                                else:
                                    socket_abcd_status.send(topic_buffer + b" " + message_buffer)
                            elif compared_abcd_status != 0 and compared_abcd_events != 0 and \
                                (compared_waan_status == 0 or compared_waan_events == 0) and \
                                 compared_abcd_data != 0 and compared_zipped != 0:

                                counter_packets_waan_status += 1
    
                                if counter_topics < args.skip_packets:
                                    logging.debug("Skipping packet")
                                elif args.enable_waan_status:
                                    socket_waan_status.send(topic_buffer + b" " + message_buffer)
                            # If it is a data packet send it through the data socket...
                            elif compared_abcd_status != 0 and compared_abcd_events != 0 and \
                                 compared_waan_status != 0 and compared_waan_events != 0 and \
                                 compared_abcd_data == 0 and compared_zipped != 0:

                                counter_packets_abcd_data += 1
    
                                if counter_topics < args.skip_packets:
                                    logging.debug("Skipping packet")
                                else:
                                    socket_abcd_data.send(topic_buffer + b" " + message_buffer)
                            # If it is a compressed packet send it through the data socket...
                            elif compared_abcd_status != 0 and compared_abcd_events != 0 and \
                                 compared_waan_status != 0 and compared_waan_events != 0 and \
                                 compared_abcd_data != 0 and compared_zipped == 0:

                                counter_packets_zipped += 1

                                if counter_topics < args.skip_packets:
                                    logging.debug("Skipping packet")
                                else:
                                    socket_abcd_data.send(topic_buffer + b" " + message_buffer)
                            else:
                                counter_packets_unknown += 1
    
                                logging.warning("Unknown packet type, skipping it.")
    
                            logging.debug("packets: {:d}; status packets: {:d}; data packets: {:d}; compressed packets: {:d}; unknown packets: {:d}".format(counter_topics, counter_packets_abcd_status, counter_packets_abcd_data, counter_packets_zipped, counter_packets_unknown))

                            # We do not use greater than equal because we already have incremented the counter
                            if counter_topics > args.skip_packets:
                                time.sleep(args.base_period / 1000)

                        topic_buffer = b""

                    byte = input_file.read(1)
    except KeyboardInterrupt:
        pass

    socket_abcd_status.close()
    socket_abcd_data.close()
