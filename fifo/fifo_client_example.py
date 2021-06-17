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

import datetime
import argparse
import zmq
import json
import time
import base64

parser = argparse.ArgumentParser(description='Send a request to fifo and reads the reply')
parser.add_argument('-R',
                    '--request_socket',
                    type = str,
                    default = "tcp://127.0.0.1:16199",
                    help = 'Request socket address')
parser.add_argument('-f',
                    '--from_date',
                    type = str,
                    default = None,
                    help = 'From date and time, default: now - 60 s')
parser.add_argument('-t',
                    '--to_date',
                    type = str,
                    default = None,
                    help = 'To date and time, default: now')

args = parser.parse_args()

print("Connecting to: {}".format(args.request_socket))

with zmq.Context() as context:
    socket = context.socket(zmq.REQ)

    socket.connect(args.request_socket)

    request_message = dict()
    request_message["msg_ID"] = 1
    request_message["timestamp"] = datetime.datetime.now().isoformat()
    request_message["command"] = "get_data"
    request_message["arguments"] = dict()

    if args.from_date is not None:
        request_message["arguments"]["from"] = args.from_date
    else:
        request_message["arguments"]["from"] = (datetime.datetime.now() - datetime.timedelta(seconds = -60)).isoformat()

    if args.to_date is not None:
        request_message["arguments"]["to"] = args.to_date
    else:
        request_message["arguments"]["to"] = datetime.datetime.now().isoformat()

    json_request_message = json.dumps(request_message)
    print("Sending message: {}".format(json_request_message))

    socket.send(json_request_message.encode('ascii'))

    json_reply_message = socket.recv().decode('ascii')

    reply_message = json.loads(json_reply_message)

    print("Received reply type: {}".format(reply_message["type"]))
    print("               id: {:d}".format(int(reply_message["msg_ID"])))
    print("               timestamp: {}".format(reply_message["timestamp"]))
    print("               size: {:d}".format(int(reply_message["size"])))
    print("               data_packets: {:d}".format(len(reply_message["data"])))

    for data_packet in reply_message["data"]:
        data = base64.b64decode(data_packet.encode('ascii'), validate = False)

        # Do something with the data packet...

    socket.close()
