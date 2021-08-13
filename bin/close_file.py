#! /usr/bin/env python3

#  (C) Copyright 2016,2019,2020,2021 Cristiano Lino Fontana
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

COMMANDS_SOCKET = "tcp://127.0.0.1:16186"

parser = argparse.ArgumentParser(description='Send command to dasa to close files')
parser.add_argument('-C',
                    '--commands_socket',
                    type = str,
                    default = COMMANDS_SOCKET,
                    help = 'Commands socket address (default: {})'.format(COMMANDS_SOCKET))

args = parser.parse_args()

print("Connecting to: {}".format(args.commands_socket))

with zmq.Context() as context:
    push_socket = context.socket(zmq.PUSH)

    push_socket.connect(args.commands_socket)

    message = dict()
    message["msg_ID"] = 1
    message["timestamp"] = datetime.datetime.now().isoformat()
    message["command"] = "stop"
    message["arguments"] = dict()

    json_message = json.dumps(message)
    print("Sending message: {}".format(json_message))

    message = push_socket.send(json_message.encode('ascii'))

    time.sleep(0.5)

    push_socket.close()
