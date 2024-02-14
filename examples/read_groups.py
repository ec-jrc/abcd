#! /usr/bin/env python3

#  (C) Copyright 2016, 2019 Cristiano Lino Fontana
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
import os

import numpy as np
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser(description='Read an events file that was analyzed by cofi and stores the coincidence groups. WARNING: Nothing is done on the groups, customize the program to do something with them.')
parser.add_argument('file_name',
                    type = str,
                    help = 'Input file name')
parser.add_argument('-m',
                    '--minimum_multiplicity',
                    type = int,
                    default = 4,
                    help = 'Minimum multiplicity to accept a group')

args = parser.parse_args()

event_PSD_dtype = np.dtype([('timestamp', np.uint64),
                            ('qshort', np.uint16),
                            ('qlong', np.uint16),
                            ('baseline', np.uint16),
                            ('channel', np.uint8),
                            ('group_counter', np.uint8),
                            ])

data_groups = list()
counter_groups = 0
counter_events_in_groups = 0

print("Reading: {}".format(args.file_name))

data_list = np.fromfile(args.file_name, dtype = event_PSD_dtype)

print("    Total number of events: {:d}".format(len(data_list)))

# Adding one because the group_counter does not take into account the reference channel event
multiplicities = (data_list['group_counter'] + 1)

print("    Minimum number of events per group: {:f}".format(multiplicities.min()))
print("    Maximum number of events per group: {:f}".format(multiplicities.max()))
print("    Average number of events per group: {:f}".format(multiplicities.mean()))

multiplicity_spectrum, edges = np.histogram(multiplicities,
                                            bins = (multiplicities.max() + 1),
                                            range = (0, multiplicities.max() + 1))

fig = plt.figure()

multiplicity_ax = fig.add_subplot(111)
multiplicity_ax.hist(edges[:-1], edges, weights=multiplicity_spectrum)
multiplicity_ax.set_xlabel('Multiplicity')
multiplicity_ax.set_ylabel('Counts')
multiplicity_ax.set_yscale('log')
multiplicity_ax.grid()

plt.show()

index_global = 0

while index_global < len(data_list):
    event = data_list[index_global]
    group_counter = event['group_counter']

    if (group_counter + 1) >= args.minimum_multiplicity:
        counter_groups += 1
        counter_events_in_groups += 1 + group_counter
        group = list()

        group.append(event)

        for index_group in range(1, 1 + group_counter):
            group.append(data_list[index_global + index_group])

        data_groups.append(group)

    index_global += 1 + group_counter

print("    Number of selected groups: {:d}".format(counter_groups))
print("    Number of events in the selected groups: {:d}".format(counter_events_in_groups))

################################################################################
# Here be somenthing to do with the groups!!!
################################################################################
