#! /usr/bin/env python3

#  (C) Copyright 2024, European Union, Cristiano Lino Fontana
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
import datetime
import logging
import zmq
import json

topic = "events".encode('ascii')

parser = argparse.ArgumentParser(description='Reads and logs ABCD events from multiple status sockets')
parser.add_argument('-S',
                    '--status_socket',
                    type = str,
                    nargs = "+",
                    default = "tcp://127.0.0.1:16180",
                    help = 'Status socket address')
parser.add_argument('-T',
                    '--polling_time',
                    type = float,
                    default = 200,
                    help = 'Socket polling time')
parser.add_argument('-o',
                    '--output_file',
                    type = str,
                    default = "ABCD_status_events_{}.tsv".format(datetime.datetime.now().strftime("%Y%m%d")),
                    help = 'Output file')
parser.add_argument('-v',
                    '--verbose',
                    action = "store_true",
                    help = 'Set verbose execution')

args = parser.parse_args()

if args.verbose:
    logging.basicConfig(level=logging.DEBUG)
else:
    logging.basicConfig(level=logging.INFO)

logging.info("Saving to: {}".format(args.output_file))
logging.info("Connecting to: {}".format(args.status_socket))
logging.debug("Topic: {}".format(topic))

with open(args.output_file, 'a') as output_file:
    output_file.write('# module\ttimestamp\tevent type\tdescription\ttopic\n')
    output_file.write('logger\t{}\tevent\tOpening log file: {}\t\n'.format(datetime.datetime.now().isoformat(), args.output_file))

with zmq.Context() as context:
    poller = zmq.Poller()

    sockets_status = list()

    for socket_address in args.status_socket:
        socket_status = context.socket(zmq.SUB)

        socket_status.connect(socket_address)
        socket_status.setsockopt(zmq.SUBSCRIBE, topic)

        sockets_status.append(socket_status)

        poller.register(socket_status, zmq.POLLIN)

    counter_messages = 0
    try:
        while True:
            socks = dict(poller.poll(args.polling_time))

            for socket_status in sockets_status:
                if socket_status in socks and socks[socket_status] == zmq.POLLIN:
                    message = socket_status.recv().decode('ascii')
                    topic, json_message = message.split(' ', 1)
            
                    event = json.loads(json_message)

                    counter_messages += 1

                    logging.debug("Received message: {:d} with topic: {}".format(counter_messages, topic))

                    try:
                        with open(args.output_file, 'a') as output_file:
                            string_output = '{}\t{}\t{}\t{}\t{}'.format(event["module"], event["timestamp"], event["type"], event[event["type"]], topic)

                            output_file.write(string_output + '\n')
                            logging.debug(string_output)
                    except KeyError:
                        pass

    except KeyboardInterrupt:
        for socket_status in sockets_status:
            socket_status.close()
