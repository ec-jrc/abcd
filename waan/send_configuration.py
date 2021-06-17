#! /usr/bin/env python3

#  (C) Copyright 2021, European Union, Cristiano Lino Fontana
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
                    default = "tcp://127.0.0.1:16208",
                    help = 'Commands socket address')
parser.add_argument('config_file',
                    type = str,
                    default = None,
                    help = 'JSON formatted config file')

args = parser.parse_args()

config = dict()

print("Reading: {}".format(args.config_file))

with open(args.config_file) as config_file:
    config = json.load(config_file)

with zmq.Context() as context:
    print("Connecting to: {}".format(args.commands_socket))

    socket = context.socket(zmq.PUSH)

    socket.connect(args.commands_socket)

    message = dict()
    message["msg_ID"] = 1
    message["timestamp"] = datetime.datetime.now().isoformat()
    message["command"] = "reconfigure"
    message["arguments"] = {"config": config}

    json_message = json.dumps(message)
    print("Sending message: {}".format(json_message))

    message = socket.send(json_message.encode('ascii'))

    time.sleep(0.5)

    socket.close()
