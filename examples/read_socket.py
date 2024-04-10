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

parser = argparse.ArgumentParser(description='Read and print ABCD messages')
parser.add_argument('-S',
                    '--socket',
                    type = str,
                    help = 'Socket address',
                    default = "tcp://127.0.0.1:16180")
parser.add_argument('-t',
                    '--topic',
                    type = str,
                    default = "",
                    help = 'Topic to subscribe to, default: ""')
parser.add_argument('-T',
                    '--polling_time',
                    type = float,
                    default = 200,
                    help = 'Socket polling time')

args = parser.parse_args()

topic = args.topic.encode('ascii')

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
                except:
                    pass

                msg_counter += 1
                    

    except KeyboardInterrupt:
        socket.close()
