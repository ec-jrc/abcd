#! /usr/bin/env python3
#
#  (C) Copyright 2019 Cristiano Lino Fontana
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
import serial


BAUD_RATE = 115200
DATA_BITS = 8
STOP_BITS = 1
PARITY = "0"
TIMEOUT = None

parser = argparse.ArgumentParser(description='Interfaces with a CAEN A7585DU')
parser.add_argument('device',
                    type = str,
                    nargs = '?',
                    default = None,
                    help = 'Serial port device for communication over USB')
parser.add_argument('-c',
                    '--command',
                    type = str,
                    default = None,
                    help = 'Send a specific command')
parser.add_argument('-l',
                    '--list_ports',
                    action = "store_true",
                    help = 'List available ports')
parser.add_argument('-t',
                    '--timeout',
                    default = None,
                    help = 'Set a timeout for the reception')
parser.add_argument('--baud_rate',
                    type = int,
                    default = BAUD_RATE,
                    help = 'Baud rate in bps (default: {:d})'.format(BAUD_RATE))
parser.add_argument('--data_bits',
                    type = int,
                    default = DATA_BITS,
                    help = 'Number of data bits (default: {:d})'.format(DATA_BITS))
parser.add_argument('--stop_bits',
                    type = int,
                    default = STOP_BITS,
                    help = 'Number of stop bits (default: {:d})'.format(STOP_BITS))
parser.add_argument('--parity',
                    type = str,
                    default = PARITY,
                    help = 'Enable parity checking (default: {})'.format(PARITY))

args = parser.parse_args()

baud_rate = args.baud_rate
data_bits = serial.EIGHTBITS
stop_bits = serial.STOPBITS_ONE
parity = serial.PARITY_NONE

if args.data_bits == 5:
    data_bits = serial.FIVEBITS
elif args.data_bits == 6:
    data_bits = serial.SIXBITS
elif args.data_bits == 7:
    data_bits = serial.SEVENBITS
elif args.data_bits == 8:
    data_bits = serial.EIGHTBITS

if args.stop_bits == 1:
    stop_bits = serial.STOPBITS_ONE
elif args.data_bits == 1.5:
    stop_bits = serial.STOPBITS_ONE_POINT_FIVE
elif args.data_bits == 2:
    stop_bits = serial.STOPBITS_TWO

if args.parity.upper() == "NONE" or args.parity == "0" or args.parity == 0:
    parity = serial.PARITY_NONE
elif args.parity.upper() == "EVEN":
    parity = serial.PARITY_EVENT
elif args.parity.upper() == "ODD":
    parity = serial.PARITY_ODD
elif args.parity.upper() == "MARK":
    parity = serial.PARITY_MARK
elif args.parity.upper() == "SPACE":
    parity = serial.PARITY_SPACE

if args.list_ports:
    from serial.tools import list_ports
    
    for index, serial_port in enumerate(list_ports.comports()):
        try:
            print("Index: {:d}".format(index))
            print("\tDevice: {}".format(serial_port.device))
            print("\tName: {}".format(serial_port.name))
            print("\tDescription: {}".format(serial_port.description))
            print("\tHWID: {}".format(serial_port.hwid))
            print("\tUSB vendor ID: {}".format(serial_port.vid))
            print("\tUSB product ID: {}".format(serial_port.pid))
            print("\tUSB serial number: {}".format(serial_port.serial_number))
            print("\tUSB location: {}".format(serial_port.location))
            print("\tUSB manufacturer: {}".format(serial_port.manufacturer))
            print("\tUSB product: {}".format(serial_port.product))
            print("\tUSB interface: {}".format(serial_port.interface))
        except Exception as e:
            print("WARNING: {}".format(e))

if args.device is None and not args.list_ports:
    parser.print_help()
elif args.device is not None:
    with serial.Serial(port = args.device,
                       baudrate = baud_rate,
                       bytesize = data_bits,
                       stopbits = stop_bits,
                       parity = parity,
                       timeout = args.timeout) as serial_port:

        print("Name: {}".format(serial_port.name))

        print("Enabling the machine interface")
        serial_port.write("AT+MACHINE\r\n".encode('ascii'))
        serial_port.flush()

        if args.command is not None:
            print("Sending command: {}".format(args.command))

            binary_command = str(args.command + '\r\n').encode('ascii')
            serial_port.write(binary_command)
            serial_port.flush()

            line = serial_port.readline().decode('ascii').strip()

            print("Received: {}".format(line))
