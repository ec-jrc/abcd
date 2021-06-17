#! /usr/bin/env python3

import argparse
import zmq
import struct
import time

import numpy as np

SUB_SOCKET = 'tcp://127.0.0.1:16181'
PUB_SOCKET = 'tcp://*:16198'
CHANNEL = 'all'
SCALING = 1
TOPIC = 'data_abcd'.encode('ascii')

###################################
# Here be the command line parser #
###################################

parser = argparse.ArgumentParser(description='Rescales the timestamp word of each event')
parser.add_argument('-S',
                    '--sub_socket',
                    type = str,
                    default = SUB_SOCKET,
                    help = 'SUB socket address, default: {}'.format(SUB_SOCKET))
parser.add_argument('-P',
                    '--pub_socket',
                    type = str,
                    default = PUB_SOCKET,
                    help = 'PUB socket address, default: {}'.format(PUB_SOCKET))
parser.add_argument('scaling',
                    type = float,
                    default = SCALING,
                    help = 'Scaling factor (default: {:d})'.format(SCALING))
#parser.add_argument('-c',
#                    '--channel',
#                    type = str,
#                    default = CHANNEL,
#                    help = 'Channel number (default: {:f})'.format(CHANNEL))

args = parser.parse_args()

scaling = args.scaling

event_PSD_dtype = np.dtype([('timestamp', np.uint64),
                            ('qshort', np.uint16),
                            ('qlong', np.uint16),
                            ('baseline', np.uint16),
                            ('channel', np.uint8),
                            ('pur', np.uint8),
                            ])


##############################
# Here be the socket reading #
##############################

with zmq.Context() as context:

    sub_socket = context.socket(zmq.SUB)
    pub_socket = context.socket(zmq.PUB)

    sub_socket.connect(args.sub_socket)

    pub_socket.bind(args.pub_socket)

    sub_socket.setsockopt(zmq.SUBSCRIBE, TOPIC)

    msg_ID = 0

    try:
        while True:
            message = sub_socket.recv()

            start_time = time.perf_counter()

            binary_topic, binary_message = message.split(b' ', 1)
            topic = binary_topic.decode('ascii', 'ignore')

            binary_message = bytearray(binary_message)

            print("Topic: {}".format(topic))

            events = 0
            offset = 0

            if topic.startswith("data_abcd_events"):

                while offset < len(binary_message):
                    #print("Offset: {:d}".format(offset))

                    timestamp, = struct.unpack("<Q", binary_message[offset:offset + 8])

                    new_timestamp = int(timestamp * scaling)
                
                    #print("Timestamp: {:d} to {:d}".format(timestamp, new_timestamp))

                    binary_timestamp = struct.pack("<Q", new_timestamp)

                    binary_message[offset:offset + 8] = binary_timestamp

                    offset += 16
                    events += 1

            elif topic.startswith("data_abcd_waveforms"):

                while offset < len(binary_message):
                    #print("Offset: {:d}".format(offset))

                    timestamp, = struct.unpack("<Q", binary_message[offset:offset + 8])
                    #channel, = struct.unpack("<B", binary_message[offset + 8:offset + 9])
                    samples_number, = struct.unpack("<I", binary_message[offset + 9:offset + 13])
                    gates_number, = struct.unpack("<B", binary_message[offset + 13:offset + 14])

                    new_timestamp = int(timestamp * scaling)

                    #print("Timestamp: {:d} to {:d}".format(timestamp, new_timestamp))

                    binary_timestamp = struct.pack("<Q", new_timestamp)

                    binary_message[offset:offset + 8] = binary_timestamp

                    offset += 14 + 2 * samples_number + samples_number * gates_number
                    events += 1

            pub_socket.send(binary_topic + b' ' + binary_message)

            end_time = time.perf_counter()
            execution_time = (end_time - start_time) * 1000
            rate = events / execution_time

            print("Execution time: {:f} ms, rate: {:f} kHz".format(execution_time, rate))

            msg_ID += 1


    except KeyboardInterrupt:
        sub_socket.close()
