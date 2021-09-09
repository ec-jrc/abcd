#! /usr/bin/env python3

#  (C) Copyright 2016, 2019, 2021, European Union, Cristiano Lino Fontana
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
import struct

import math
import numpy as np
import matplotlib.pyplot as plt

ENERGY_RESOLUTION = 20
ENERGY_MIN = 0
ENERGY_MAX = 60000
BASELINE_SAMPLES = 64
LONG_GATE = 64
PULSE_POLARITY = "negative"

POSITIVE = 1
NEGATIVE = -1

parser = argparse.ArgumentParser(description='Calculates the average waveform for a given energy interval.')
parser.add_argument('file_name',
                    type = str,
                    help = 'Waveforms file name')
parser.add_argument('channel',
                    type = int,
                    help = 'Channel selection')
parser.add_argument('--pulse_polarity',
                    type = str,
                    default = PULSE_POLARITY,
                    help = 'Pulse polarity (default: {})'.format(PULSE_POLARITY))
parser.add_argument('-R',
                    '--energy_resolution',
                    type = float,
                    default = ENERGY_RESOLUTION,
                    help = 'Energy resolution (default: {:.0f})'.format(ENERGY_RESOLUTION))
parser.add_argument('-e',
                    '--energy_min',
                    type = float,
                    default = ENERGY_MIN,
                    help = 'Energy min (default: {:.0f})'.format(ENERGY_MIN))
parser.add_argument('-E',
                    '--energy_max',
                    type = float,
                    default = ENERGY_MAX,
                    help = 'Energy max (default: {:.0f})'.format(ENERGY_MAX))
parser.add_argument('-B',
                    '--baseline_samples',
                    type = int,
                    default = BASELINE_SAMPLES,
                    help = 'Number of samples to average to calculate baseline (default: {:d})'.format(BASELINE_SAMPLES))
parser.add_argument('-G',
                    '--long_gate',
                    type = int,
                    default = LONG_GATE,
                    help = 'Number of samples to integrate from the end of the baseline (default: {:d})'.format(LONG_GATE))
parser.add_argument('-N',
                    '--max_number',
                    type = int,
                    default = None,
                    help = 'Maximum number of waveforms to average (optional)')
parser.add_argument('-s',
                    '--save_data',
                    action = "store_true",
                    help = 'Save histograms to file')

args = parser.parse_args()

basename, extension = os.path.splitext(args.file_name)

if extension == ".xz":
    import lzma
    this_open = lzma.open
elif extension == ".bz2":
    import bz2
    this_open = bz2.open
else:
    this_open = open

selected_channel = args.channel

pulse_polarity = NEGATIVE

if args.pulse_polarity.lower() == "negative":
    pulse_polarity = NEGATIVE
elif args.pulse_polarity.lower() == "positive":
    pulse_polarity = POSITIVE

class Waveform:
    def __init__(self):
        self.timestamp = None
        self.channel = None
        self.samples = list()
        self.gates = list()

def next_waveform(input_file):
    timestamp, = struct.unpack("<Q", input_file.read(8))
    channel, = struct.unpack("<B", input_file.read(1))
    samples_number, = struct.unpack("<I", input_file.read(4))
    gates_number, = struct.unpack("<B", input_file.read(1))

    waveform = Waveform()
    waveform.timestamp = timestamp
    waveform.channel = channel

    waveform.samples = np.asarray(struct.unpack("<" + str(samples_number) + "H",
                                                input_file.read(2 * samples_number)))

    for i in range(gates_number):
        gate = struct.unpack("<" + str(samples_number) + "B",
                             input_file.read(1 * samples_number))

        waveform.gates.append(gate)

    return waveform

def calculate_baseline(samples, baseline_samples):
    return np.average(samples[0:baseline_samples])

def calculate_integral(samples, start, end):
    return np.sum(samples[start:end])

input_file = this_open(args.file_name, "rb")

counter_total = int(0)
counter_selected = int(0)
counter_averaged = int(0)

samples_averaged = None
energies = list()

try:
    while True:
        this_waveform = next_waveform(input_file)

        counter_total += 1

        if this_waveform.channel == selected_channel:
            counter_selected += 1

            baseline = calculate_baseline(this_waveform.samples, args.baseline_samples)
            
            if pulse_polarity == NEGATIVE:
                samples = baseline - this_waveform.samples
            elif pulse_polarity == POSITIVE:
                samples = this_waveform.samples - baseline

            energy = calculate_integral(samples,
                                        args.baseline_samples, 
                                        args.baseline_samples + args.long_gate)

            energies.append(energy)

            if samples_averaged is None:
                samples_averaged = np.zeros(len(this_waveform.samples), dtype = np.float)

            if args.energy_min <= energy and energy < args.energy_max:
                counter_averaged += 1

                samples_averaged += samples / energy

                print("total: {:d}, averaged: {:d} ({:.2f}%), baseline: {:.2f}, energy: {:.0f}".format(counter_total, counter_averaged, float(counter_averaged) / counter_total * 100, baseline, energy))
        if (args.max_number is not None) and (counter_averaged >= args.max_number):
            break
except Exception as e:
    print("ERROR: {}".format(e))
except KeyboardInterrupt:
    pass
            
samples_averaged /= counter_averaged

N_E = math.floor((max(energies) - min(0, min(energies))) / args.energy_resolution)

spectrum, energy_edges = np.histogram(energies, bins = N_E)

if args.save_data:
    try:
        print("Exporting waveform...")

        basename, extension = os.path.splitext(args.file_name)

        new_file = "{}_average_wave_ch{:d}_energy{:.0f}to{:.0f}.csv".format(
                   basename,
                   selected_channel,
                   args.energy_min, args.energy_max)

        print("Exporting to {}".format(new_file))
                     
        with open(new_file, "w") as output_file:
            output_file.write("#i,averaged sample")
            output_file.write(",Baseline samples: {:d}".format(args.baseline_samples))
            output_file.write(",Long gate: {:d}".format(args.long_gate))
            output_file.write(",Energy: {:f} to {:f}".format(args.energy_min, args.energy_max))
            output_file.write(",N: {:d}".format(counter_averaged))
            output_file.write(",Pulse polarity: {}".format(args.pulse_polarity))
            output_file.write("\n")

            for index, sample in enumerate(samples_averaged):
                output_file.write("{:d},{:f}".format(index, sample))
                output_file.write("\n")

    except Exception as e:
        print("ERROR: Unable to export the average waveform: {}".format(e))
else:
    selection = np.logical_and(args.energy_min < energy_edges[:-1], energy_edges[:-1] < args.energy_max)
    
    fig = plt.figure(figsize = (8, 4.5))
    
    fig.suptitle("Average waveform of energies between {:.0f} and {:.0f} (N: {:d})".format(
                 args.energy_min, args.energy_max, counter_averaged))
    
    fig.subplots_adjust(left = 0.09, bottom = 0.11, right = 0.98, top = 0.86, hspace = 0.32)
    ax_spec = fig.add_subplot(211)
    ax_wave = fig.add_subplot(212)
    
    ax_spec.step(energy_edges[:-1],
                 spectrum,
                 color = "C0",
                 label = "Spectrum",
                 where = 'post')
    ax_spec.fill_between(energy_edges[:-1][selection],
                         spectrum[selection],
                         color = "C1",
                         label = "ROI",
                         step = "post")
    ax_spec.set_yscale('linear')
    ax_spec.grid()
    ax_spec.legend()
    ax_spec.set_xlabel("Energy [ch]")
    
    N = len(samples_averaged)
    times = np.arange(N)
    
    gate = np.zeros(N)
    gate[args.baseline_samples:args.baseline_samples + args.long_gate] = max(samples_averaged) / 2
    
    selection = np.logical_and(args.baseline_samples < times, times < args.baseline_samples + args.long_gate)
    
    ax_wave.step(times,
                 samples_averaged,
                 color = "C0",
                 label = "Samples",
                 where = 'post')
    ax_wave.step(times,
                 gate,
                 color = "C2",
                 label = "Integration gate",
                 where = 'post')
    ax_wave.fill_between(times[selection],
                         samples_averaged[selection],
                         color = "C1",
                         label = "Integration area",
                         step = "post")
    ax_wave.set_yscale('linear')
    ax_wave.grid()
    ax_wave.legend()
    ax_wave.set_xlabel("Time [ch]")
    
    plt.show()
