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

types = {'sub': zmq.SUB, 'pull': zmq.PULL}
topic = "".encode('ascii')

parser = argparse.ArgumentParser(description='Read and print commands socket, useful for debugging')
parser.add_argument('address',
                    type = str,
                    help = 'Socket address')
parser.add_argument('-t',
                    '--socket_type',
                    type = str,
                    default = 'pull',
                    help = 'Socket type, values: ' + ', '.join(types.keys()))
parser.add_argument('-T',
                    '--polling_time',
                    type = float,
                    help = 'Socket polling time',
                    default = 200)

args = parser.parse_args()

with zmq.Context() as context:
    poller = zmq.Poller()

    socket = context.socket(types[args.socket_type]);

    socket.bind(args.address)
    
    if args.socket_type == 'sub':
        socket.setsockopt(zmq.SUBSCRIBE, topic)

    poller.register(socket, zmq.POLLIN)

    try:
        while True:
            socks = dict(poller.poll(args.polling_time))

            if socket in socks and socks[socket] == zmq.POLLIN:
                message = socket.recv().decode('ascii')
                print("Message: {}".format(message))

                #topic, json_message = message.split(' ', 1)
                #status = json.loads(json_message)

    except KeyboardInterrupt:
        socket.close()
