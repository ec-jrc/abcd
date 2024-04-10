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

import os
import argparse
import zmq
import json
import csv

parser = argparse.ArgumentParser(description='Read spectra from the data socket of spec')
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

topic = "data_spec_histograms".encode('ascii')

basename, extenstion = os.path.splitext(args.output)

print("Connecting to: {}".format(args.socket))
print("Subscribing to topic: '{}'".format(topic))

with zmq.Context() as context:
    poller = zmq.Poller()

    socket = context.socket(zmq.SUB)

    socket.connect(args.socket)
    socket.setsockopt(zmq.SUBSCRIBE, topic)

    poller.register(socket, zmq.POLLIN)

    msg_counter = 0

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
                    print("\tdata length: {}".format(len(status["data"])))

                    active_channels = status["active_channels"]

                    print("\tactive channels: {}".format(active_channels));

                    for data in status["data"]:
                        channel = data["id"]

                        histo = data["energy"]

                        print("\tkeys: {}".format(list(histo.keys())))
                        print("\tkeys: {}".format(list(histo["config"].keys())))

                        min_x = float(histo['config']['min'])
                        max_x = float(histo['config']['max'])
                        bins = int(histo['config']['bins'])
                        bin_width = (max_x - min_x) / bins

                        spectrum_x = [min_x + bin_width * i for i in range(bins)]
                        spectrum_y = histo['data']

                        print("Writing spectrum of channel {:d} to file: {}_channel{:d}.csv".format(channel, basename, channel))

                        with open("{}_channel{:d}.csv".format(basename, channel), 'wb') as csv_file:
                            csv_file.write("# edge\tcounts\n".encode('ascii'))
                            for i in range(bins):
                                csv_file.write(("{:f}\t{:f}\n".format(spectrum_x[i], spectrum_y[i])).encode('ascii'))
                except Exception as e:
                    print(e)

                msg_counter += 1
                    

    except KeyboardInterrupt:
        socket.close()
