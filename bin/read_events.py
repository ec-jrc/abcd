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

import datetime
import argparse
import zmq
import json

parser = argparse.ArgumentParser(description='Read and print ABCD events from status sockets')
parser.add_argument('-S',
                    '--events_socket',
                    type = str,
                    help = 'Status socket address',
                    default = "tcp://127.0.0.1:16180")
parser.add_argument('-T',
                    '--polling_time',
                    type = float,
                    help = 'Socket polling time',
                    default = 200)
parser.add_argument('-o',
                    '--output_file',
                    type = str,
                    default = "log/events_{}.txt".format(datetime.datetime.now().strftime("%Y%m%d")),
                    help = 'Output file')

args = parser.parse_args()

topic = "events".encode('ascii')

print("Saving to: {}".format(args.output_file))
print("Connecting to: {}".format(args.events_socket))
print("Topic: {}".format(topic))

with zmq.Context() as context:
    poller = zmq.Poller()

    socket = context.socket(zmq.SUB)

    socket.connect(args.events_socket)
    socket.setsockopt(zmq.SUBSCRIBE, topic)

    poller.register(socket, zmq.POLLIN)

    i = 0
    try:
        while True:
            socks = dict(poller.poll(args.polling_time))

            if socket in socks and socks[socket] == zmq.POLLIN:
                message = socket.recv().decode('ascii')
                topic, json_message = message.split(' ', 1)
            
                event = json.loads(json_message)

                i += 1

                print("Message {:d}:".format(i))
                print("{{msg_ID: {:d},".format(event["msg_ID"]), end="")
                print("\tmodule: '{}',".format(event["module"]), end="")
                print("\ttimestamp: '{}',".format(event["timestamp"]), end="")
                try:
                    print("\ttype: '{}',\tevent: '{}'".format(event["type"], event[event["type"]]), end="")
                except KeyError:
                    pass
                print("}")

                try:
                    with open(args.output_file, 'a') as output_file:
                        output_file.write('{}\t{}\t{}\t{}\n'.format(event["timestamp"], event["module"], event["type"], event[event["type"]]))
                except KeyError:
                    pass

    except KeyboardInterrupt:
        socket.close()
