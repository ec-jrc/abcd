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

import argparse
import json
import re

programs = ['abcd', 'efg', 'hijk', 'lmno', 'pqrs', 'wafi']
define_pattern = re.compile(r'#define[ \t]+([A-Za-z_]+)[ \t]+(.+)')

parser = argparse.ArgumentParser(description='Convert defaults.h file to JSON, useful for compilation')
parser.add_argument('input_file',
                    type = str,
                    help = 'Input file')
parser.add_argument('-o',
                    '--output_file',
                    type = str,
                    help = 'Output file',
                    default = None)

args = parser.parse_args()

defaults = {program: dict() for program in programs}

with open(args.input_file, 'r') as input_file:
    for line in input_file:
        stripped_line = line.strip()

        match = define_pattern.search(stripped_line)
        if match:
            keys = match.group(1).split(r'_', 2)
            value = match.group(2).strip(r'"')

            #print('Keys: {}'.format(keys))
            #print('Value: {}'.format(value))

            try:
                if keys[0] == 'defaults' and keys[1] in programs:
                    defaults[keys[1]][keys[2]] = value
            except IndexError:
                pass

try:
    with open(args.output_file, "w") as output_file:
        json.dump(defaults, output_file, sort_keys = True, indent = 4)
except TypeError:
    print(json.dumps(defaults, sort_keys = True, indent = 4))
