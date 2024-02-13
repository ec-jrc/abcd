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

INPUT_ADDRESS = "tcp://127.0.0.1:17000"
OUTPUT_ADDRESS = "tcp://*:16191"

parser = argparse.ArgumentParser(description='Creates N input SUB sockets and forwards the messages to a single output PUB socket')
parser.add_argument('-A',
                    '--input_baseaddress',
                    type = str,
                    default = INPUT_ADDRESS,
                    help = 'Input socket address to which a number will be added, default: ""'.format(INPUT_ADDRESS))
parser.add_argument('-t',
                    '--topic',
                    type = str,
                    default = "",
                    help = 'Topic to subscribe to, default: ""')
parser.add_argument('-D',
                    '--output_address',
                    type = str,
                    default = OUTPUT_ADDRESS,
                    help = 'Output socket address, default: ""'.format(OUTPUT_ADDRESS))
parser.add_argument('-N',
                    '--number_of_inputs',
                    type = int,
                    default = 4,
                    help = 'Number of input SUB sockets')
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

logging.info("Connecting to: {}".format(args.input_baseaddress))
logging.info("Subscribing to topic: '{}'".format(topic))
logging.info("Output socket: '{}'".format(args.output_address))
logging.info("Number of outputs: '{}'".format(args.number_of_inputs))

tokens = args.input_baseaddress.split(':')

print(tokens)

if len(tokens) >= 2:
    input_address_base = ':'.join(tokens[0:-1])
    input_address_separator = ':'
    input_address_basenumber = int(tokens[-1])
else:
    input_address_base = args.input_baseaddress
    input_address_separator = ''
    input_address_basenumber = 0

logging.debug("Base address: '{}'".format(input_address_base))
logging.debug("Separator: '{}'".format(input_address_separator))
logging.debug("Base number: '{}'".format(input_address_basenumber))

with zmq.Context() as context:
    poller = zmq.Poller()

    socket_output = context.socket(zmq.PUB)

    socket_output.bind(args.output_address)

    socket_inputs = list()
    counter_messages = 0
    counters_messages = list()

    for index in range(args.number_of_inputs):
        input_address = input_address_base + input_address_separator + str(input_address_basenumber + index)
        logging.info('Creating socket: {}'.format(input_address))

        socket_input = context.socket(zmq.SUB)

        socket_input.connect(input_address)
        socket_input.setsockopt(zmq.SUBSCRIBE, topic)

        socket_inputs.append(socket_input)

        poller.register(socket_input, zmq.POLLIN)

        counters_messages.append(0)

    try:
        while True:
            socks = dict(poller.poll(args.polling_time))

            for socket_index, socket_input in enumerate(socket_inputs):
                if socket_input in socks and socks[socket_input] == zmq.POLLIN:
                    message = socket_input.recv()

                    logging.debug("Message from socket number: {:d} overall: {:d} id: {:d}".format(socket_index, counter_messages, counters_messages[socket_index]))

                    socket_output.send(message, copy = False)

                    counters_messages[socket_index] += 1
                    counter_messages += 1
                    

    except KeyboardInterrupt:
        socket_output.close()

        for i in range(args.number_of_inputs):
            socket_inputs[i].close()
