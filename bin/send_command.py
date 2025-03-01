#! /usr/bin/env python3

#  (C) Copyright 2016, 2024 European Union, Cristiano Lino Fontana
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

parser = argparse.ArgumentParser(description='Send command to ABCD modules')
parser.add_argument('-C',
                    '--commands_socket',
                    type = str,
                    default = "tcp://127.0.0.1:16182",
                    help = 'Commands socket address')
parser.add_argument('-a',
                    '--arguments',
                    type = str,
                    default = "{}",
                    help = 'Command arguments, as a JSON string')
parser.add_argument('command',
                    type = str,
                    help = 'Command')

args = parser.parse_args()

print("Connecting to: {}".format(args.commands_socket))

with zmq.Context() as context:
    socket = context.socket(zmq.PUSH)

    socket.connect(args.commands_socket)

    try:
        message = dict()
        message["msg_ID"] = 1
        message["timestamp"] = datetime.datetime.now().isoformat()
        message["command"] = args.command

        message["arguments"] = json.loads(args.arguments)

        json_message = json.dumps(message)
        print("Sending message: {}".format(json_message))

        message = socket.send(json_message.encode('ascii'))

        time.sleep(1)
    except json.decoder.JSONDecodeError as error:
        print("ERROR on JSON decoding: {}".format(error))

    socket.close()
