#! /usr/bin/env python3

#  (C) Copyright 2023, European Union, Cristiano Lino Fontana
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
import logging

INPUT_ADDRESS = "tcp://127.0.0.1:16181"
OUTPUT_ADDRESS = "tcp://*:17000"

parser = argparse.ArgumentParser(description='Creates N output PUB sockets to which it equally distributes the messages received in the input SUB socket')
parser.add_argument('-A',
                    '--input_address',
                    type = str,
                    default = INPUT_ADDRESS,
                    help = 'Input socket address, default: ""'.format(INPUT_ADDRESS))
parser.add_argument('-t',
                    '--topic',
                    type = str,
                    default = "",
                    help = 'Topic to subscribe to, default: ""')
parser.add_argument('-D',
                    '--output_baseaddress',
                    type = str,
                    default = OUTPUT_ADDRESS,
                    help = 'Output socket address to which a number will be added, default: ""'.format(OUTPUT_ADDRESS))
parser.add_argument('-N',
                    '--number_of_outputs',
                    type = int,
                    default = 4,
                    help = 'Number of output PUB sockets')
parser.add_argument('-T',
                    '--polling_time',
                    type = float,
                    default = 100,
                    help = 'Socket polling time')
parser.add_argument('-v',
                    '--verbose',
                    action = "store_true",
                    help = 'Set verbose execution')

args = parser.parse_args()

if args.verbose:
    logging.basicConfig(level=logging.DEBUG)
else:
    logging.basicConfig(level=logging.INFO)

topic = args.topic.encode('ascii')

logging.info("Connecting to: {}".format(args.input_address))
logging.info("Subscribing to topic: '{}'".format(topic))
logging.info("Output socket: '{}'".format(args.output_baseaddress))
logging.info("Number of outputs: '{}'".format(args.number_of_outputs))

tokens = args.output_baseaddress.split(':')

print(tokens)

if len(tokens) >= 2:
    output_address_base = ':'.join(tokens[0:-1])
    output_address_separator = ':'
    output_address_basenumber = int(tokens[-1])
else:
    output_address_base = args.output_baseaddress
    output_address_separator = ''
    output_address_basenumber = 0

logging.debug("Base address: '{}'".format(output_address_base))
logging.debug("Separator: '{}'".format(output_address_separator))
logging.debug("Base number: '{}'".format(output_address_basenumber))

with zmq.Context() as context:
    poller = zmq.Poller()

    socket_input = context.socket(zmq.SUB)

    socket_input.connect(args.input_address)
    socket_input.setsockopt(zmq.SUBSCRIBE, topic)

    socket_outputs = list()

    for i in range(args.number_of_outputs):
        output_address = output_address_base + output_address_separator + str(output_address_basenumber + i)
        logging.info('Creating socket: {}'.format(output_address))

        socket_output = context.socket(zmq.PUB)
        socket_output.bind(output_address)

        socket_outputs.append(socket_output)

    poller.register(socket_input, zmq.POLLIN)

    counter_messages = 0
    try:
        while True:
            socks = dict(poller.poll(args.polling_time))

            if socket_input in socks and socks[socket_input] == zmq.POLLIN:
                message = socket_input.recv()

                logging.debug("Message id: {:d} to socket number: {:d}".format(counter_messages, counter_messages % args.number_of_outputs))

                socket_outputs[counter_messages % args.number_of_outputs].send(message, copy = False)

                counter_messages += 1
                    

    except KeyboardInterrupt:
        socket_input.close()

        for i in range(args.number_of_outputs):
            socket_outputs[i].close()
