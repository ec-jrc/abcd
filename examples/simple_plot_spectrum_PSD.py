#! /usr/bin/env python3

#  (C) Copyright 2016, 2019, 2025 Cristiano Lino Fontana
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

import argparse # for parsing command line arguments
import numpy as np # for numerical operations and data reading
import matplotlib.pyplot as plt # for plotting
from matplotlib.colors import LogNorm # for logaritmic color scales

# Default values for histograms bins and ranges
PSD_BINS = 200
PSD_MIN = -0.1
PSD_MAX = 0.9
ENERGY_BINS = 1100
ENERGY_MIN = 0
ENERGY_MAX = 66000

# Set up the argument parser for the command line user input
# argparse also automatically generates the inline help when the program is called with the '-h' option
parser = argparse.ArgumentParser(description='Plots spectra and Pulse Shape Discrimination information from ABCD ade data files.')
parser.add_argument('file_name', type = str, help = 'ade file name')
parser.add_argument('channel', type = int, help = 'Channel selection')
parser.add_argument('-n', '--PSD_bins', type = int, default = PSD_BINS, help = f'Number of bins for the PSD (default: {PSD_BINS})')
parser.add_argument('-p', '--PSD_min', type = float, default = PSD_MIN, help = f'PSD min (default: {PSD_MIN})')
parser.add_argument('-P', '--PSD_max', type = float, default = PSD_MAX, help = f'PSD max (default: {PSD_MAX})')
parser.add_argument('-N', '--energy_bins', type = int, default = ENERGY_BINS, help = f'Number of bins for the energy spectrum (default: {ENERGY_BINS})')
parser.add_argument('-e', '--energy_min', type = float, default = ENERGY_MIN, help = f'Energy min (default: {ENERGY_MIN})')
parser.add_argument('-E', '--energy_max', type = float, default = ENERGY_MAX, help = f'Energy max (default: {ENERGY_MAX})')

# Parse the command line arguments and store them in the 'args' variable
args = parser.parse_args()

# Define the binary format of the ade file
event_PSD_dtype = np.dtype([('timestamp', np.uint64),
                            ('qshort', np.uint16),
                            ('qlong', np.uint16),
                            ('baseline', np.uint16),
                            ('channel', np.uint8),
                            ('group_counter', np.uint8)])

print(f"Reading: {args.file_name})")

# Read the data from the binary file
data = np.fromfile(args.file_name, dtype = event_PSD_dtype)

# Select the data corresponding to the selected channel
# Also discard events with qlong == 0, as they would give an error in the PSD determination
selection = (data['channel'] == args.channel) & (data['qlong'] != 0)

# Read the values as simple arrays
qlongs = data['qlong'][selection]
qshorts = data['qshort'][selection]
PSDs = (qlongs.astype(np.float64) - qshorts) / qlongs
# In case the timestamps are needed...
#timestamps = data['timestamp'][selection]

# Calculate the energy spectrum and the PSD vs energy diagram
spectrum_energy, energy_edges = np.histogram(qlongs,
                                             bins = args.energy_bins,
                                             range = (args.energy_min, args.energy_max))
histo2d, PSD_edges, energy_edges  = np.histogram2d(PSDs, qlongs,
                                                   bins = (args.PSD_bins, args.energy_bins),
                                                   range = ((args.PSD_min, args.PSD_max), (args.energy_min, args.energy_max)))

# Create the figure on which the plots will be shown
fig = plt.figure()

# The figure contains two plots (axes in matplotlib terms)
spect_energy_ax = fig.add_subplot(211)
histo2d_ax = fig.add_subplot(212, sharex = spect_energy_ax)

# Plot the energy spectrum
spect_energy_ax.step(energy_edges[:-1], spectrum_energy, label = "Energy spectrum", where = 'post')

spect_energy_ax.set_xlim(args.energy_min, args.energy_max)
spect_energy_ax.set_yscale('log')
spect_energy_ax.set_xlabel('Energy [ch]')
spect_energy_ax.set_ylabel('Counts')
spect_energy_ax.grid()
spect_energy_ax.legend()

# Plot the PSD vs energy diagram
histo2d_ax.imshow(histo2d,
                  origin = 'lower',
                  norm = LogNorm(),
                  interpolation = 'none',
                  extent = (min(energy_edges), max(energy_edges),
                            min(PSD_edges), max(PSD_edges)),
                  aspect = 'auto')

histo2d_ax.set_xlabel('Energy [ch]')
histo2d_ax.set_ylabel('PSD')
histo2d_ax.grid()

# Automagically tune the plots placing to make it look nice
plt.tight_layout()

# Actually show the plots
plt.show()
