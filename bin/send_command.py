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
import time

parser = argparse.ArgumentParser(description='Send command to ABCD modules')
parser.add_argument('-C',
                    '--commands_socket',
                    type = str,
                    default = "tcp://127.0.0.1:16182",
                    help = 'Commands socket address')
parser.add_argument('-a',
                    '--arguments',
                    type = str,
                    default = None,
                    help = 'Command arguments, as a JSON string')
parser.add_argument('command',
                    type = str,
                    help = 'Command')

args = parser.parse_args()

print("Connecting to: {}".format(args.commands_socket))

# On python2.7 zmq.context is not compatible with the 'with' statement
context = zmq.Context()
if True:
#with zmq.Context() as context:
    socket = context.socket(zmq.PUSH)

    socket.connect(args.commands_socket)

    message = dict()
    message["msg_ID"] = 1
    message["timestamp"] = datetime.datetime.now().isoformat()
    message["command"] = args.command

    try:
        message["arguments"] = json.loads(args.arguments)
    except Exception:
        message["arguments"] = None

    json_message = json.dumps(message)
    print("Sending message: {}".format(json_message))

    message = socket.send(json_message.encode('ascii'))

    time.sleep(0.5)

    socket.close()

context.destroy()
