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
import zmq
import json

import numpy as np

TOPIC = "data_abcd"
ADDRESS_INPUT = "tcp://127.0.0.1:16181"
ADDRESS_OUTPUT = "tcp://127.0.0.1:19181"

parser = argparse.ArgumentParser(description='Read and forward ABCD data sockets')
parser.add_argument('-A',
                    '--input_socket',
                    type = str,
                    default = ADDRESS_INPUT,
                    help = f'Input socket address, default: {ADDRESS_INPUT}')
parser.add_argument('-D',
                    '--output_socket',
                    type = str,
                    default = ADDRESS_OUTPUT,
                    help = f'Output socket address, default: {ADDRESS_OUTPUT}')
parser.add_argument('-t',
                    '--topic',
                    type = str,
                    default = TOPIC,
                    help = f'Topic to subscribe to, default: "{TOPIC}"')
parser.add_argument('-T',
                    '--polling_time',
                    type = float,
                    help = 'Socket polling time',
                    default = 200)

args = parser.parse_args()

topic = args.topic.encode('ascii')

print(f"Input socket: {args.input_socket}")
print(f"Output socket: {args.output_socket}")
print(f"Subscribing to topic: '{topic}'")

event_PSD_dtype = np.dtype([('timestamp', np.uint64),
                            ('qshort', np.uint16),
                            ('qlong', np.uint16),
                            ('baseline', np.uint16),
                            ('channel', np.uint8),
                            ('pur', np.uint8),
                            ])

with zmq.Context() as context:
    poller = zmq.Poller()

    socket_input = context.socket(zmq.SUB)
    socket_input.connect(args.input_socket)
    socket_input.setsockopt(zmq.SUBSCRIBE, args.topic.encode('ascii'))

    socket_output = context.socket(zmq.PUB)
    socket_output.connect(args.output_socket)

    poller.register(socket_input, zmq.POLLIN)

    counter_msg = 0
    try:
        while True:
            socks = dict(poller.poll(args.polling_time))

            if socket_input in socks and socks[socket_input] == zmq.POLLIN:
                message = socket_input.recv()

                print(f"Message [{counter_msg}]")

                try:
                    separator_index = message.find(b' ')

                    topic = message[:separator_index].decode('ascii')

                    print("\tTopic: {}".format(topic))
            
                    if topic.startswith("data_abcd_events"):
                        buffer = message[separator_index+1:]
                        data = np.frombuffer(buffer, dtype = event_PSD_dtype)

                        # Do something here with the data
                        #timestamps = data['timestamp']
                        #channels = data['channel']
                        #...

                    socket_output.send(topic.encode('ascii') + b' ' + buffer)
                except Exception as error:
                    print(error)

                counter_msg += 1
                    

    except KeyboardInterrupt:
        socket_input.close()
        socket_output.close()
