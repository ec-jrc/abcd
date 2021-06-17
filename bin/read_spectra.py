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

from __future__ import print_function, with_statement

import os
import argparse
import zmq
import json
import csv

parser = argparse.ArgumentParser(description='Read PQRS spectra from the data socket')
parser.add_argument('-S',
                    '--socket',
                    type = str,
                    help = 'Socket address',
                    default = "tcp://127.0.0.1:16188")
parser.add_argument('-T',
                    '--polling_time',
                    type = float,
                    default = 200,
                    help = 'Socket polling time')
parser.add_argument('output',
                    type = str,
                    help = 'Output file')

args = parser.parse_args()

basename, extenstion = os.path.splitext(args.output)

topic = "data_pqrs_histograms".encode('ascii')

print("Connecting to: {}".format(args.socket))
print("Subscribing to topic: {}".format(topic))

with zmq.Context() as context:
    poller = zmq.Poller()

    socket = context.socket(zmq.SUB)

    socket.connect(args.socket)
    socket.setsockopt(zmq.SUBSCRIBE, topic)

    poller.register(socket, zmq.POLLIN)

    msg_counter = 0
    active_channels = set()
    spectra_x = dict()
    spectra_y = dict()

    try:
        while True:
            socks = dict(poller.poll(args.polling_time))

            if socket in socks and socks[socket] == zmq.POLLIN:
                message = socket.recv()

                print("Message [{:d}]:".format(msg_counter))

                try:
                    topic, json_message = message.decode('ascii', 'ignore').split(' ', 1)

                    print("\tTopic: {}".format(topic))
            
                    status = json.loads(json_message)

                    print("\tmsg_ID: {:d}".format(status["msg_ID"]))
                    print("\tdatetime: {}".format(status["timestamp"]))
                    print("\tkeys: {}".format(list(status.keys())))

                    these_active_channels = status["active_channels"]

                    print("\tactive channels: {}".format(these_active_channels));

                    active_channels.union(these_active_channels)

                    for channel in these_active_channels:
                        data = status["data"]["qlong"]["{:d}".format(channel)]

                        min_x = float(data['config']['min'])
                        bins = int(data['config']['bins'])
                        bin_width = float(data['config']['bin_width'])

                        spectra_x[channel] = [min_x + bin_width * i for i in range(bins)]
                        spectra_y[channel] = data['histo']

                        print("Writing spectrum of channel {:d} to file: {}_channel{:d}.csv".format(channel, basename, channel))
                        with open("{}_channel{:d}.csv".format(basename, channel), 'wb') as csv_file:
                            writer = csv.writer(csv_file, delimiter='\t')

                            for i in range(bins):
                                writer.writerow((spectra_x[channel][i], spectra_y[channel][i]))
                except:
                    pass

                msg_counter += 1
                    

    except KeyboardInterrupt:
        socket.close()
