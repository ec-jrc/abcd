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
import struct

import tkinter
import numpy as np
import numpy.fft as fft
import matplotlib.pyplot as plt

from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

# Compatibility with old API for Debian
try:
    from matplotlib.backends.backend_tkagg import NavigationToolbar2Tk
except:
    from matplotlib.backends.backend_tkagg import NavigationToolbar2TkAgg as NavigationToolbar2Tk

# Implement the default Matplotlib key bindings.
from matplotlib.backend_bases import key_press_handler

parser = argparse.ArgumentParser(description='Plots waveforms from ABCD waveforms data files.\n' +
    "Pressing the left and right keys shows the previous or next waveform.\n" +
    "Pressing the 'e' key exports the current waveform to a CSV file.")
parser.add_argument('file_name',
                    type = str,
                    help = 'Waveforms file name')
parser.add_argument('-c',
                    '--channel',
                    type = int,
                    default = None,
                    help = 'Channel selection')
parser.add_argument('-n',
                    '--waveform_number',
                    type = int,
                    default = None,
                    help = 'Plots the Nth waveform')
parser.add_argument('--clock_step',
                    type = float,
                    default = 2,
                    help = 'Step of the ADC sampling of the waveform in ns (default: 2 ns)')
parser.add_argument('--baseline_samples',
                    type = int,
                    default = 700,
                    help = 'Step of the ADC sampling of the waveform in ns (default: 2 ns)')

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

input_file = this_open(args.file_name, "rb")

selected_channel = args.channel
show_transform = False
previous_xlims = None
waveforms_buffer = list()
waveform_index = -1
if args.waveform_number is not None:
    waveform_index = args.waveform_number - 1

def next_waveform():
    global waveforms_buffer
    global waveform_index
    global input_file

    if (waveform_index + 1) < len(waveforms_buffer):
        waveform_index += 1

        if waveforms_buffer[waveform_index]["channel"] == selected_channel or selected_channel is None:
            return waveforms_buffer[waveform_index]
        elif selected_channel is not None:
            return next_waveform()
    else:
        print("Reading a new waveform")
        try:
            timestamp, = struct.unpack("<Q", input_file.read(8))
            channel, = struct.unpack("<B", input_file.read(1))
            samples_number, = struct.unpack("<I", input_file.read(4))
            gates_number, = struct.unpack("<B", input_file.read(1))

            samples = struct.unpack("<" + str(samples_number) + "H",
                                    input_file.read(2 * samples_number))

            gates = list()

            for i in range(gates_number):
                gate = struct.unpack("<" + str(samples_number) + "B",
                                     input_file.read(1 * samples_number))

                gates.append(gate)

            waveforms_buffer.append({"timestamp": timestamp,
                                     "channel": channel,
                                     "samples": samples,
                                     "gates": gates,
                                    })

            # Returning the next_waveform() without increasing the index
            # should return the waveform that we just read
            return next_waveform()
        except Exception as e:
            print("ERROR: Unable to read the new waveform: {}".format(e))
            return None

def curr_waveform():
    global waveforms_buffer
    global waveform_index

    return waveforms_buffer[waveform_index]

def prev_waveform():
    global waveforms_buffer
    global waveform_index

    if waveform_index > 0:
        waveform_index -= 1

        if waveforms_buffer[waveform_index]["channel"] == selected_channel or selected_channel is None:
            return waveforms_buffer[waveform_index]
        elif selected_channel is not None:
            return prev_waveform()
    else:
        print("Already at the first waveform")
        return None

def plot_update(waveform):
    global show_transform

    if not show_transform:
        plot_waveform(waveform)
    else:
        plot_transform(waveform)

def plot_waveform(waveform):
    global waveform_index
    global args
    global previous_xlims
    global canvas
    global ax_wave
    global ax_gate

    N = len(waveform["samples"])

    if previous_xlims == None:
        previous_xlims = (0, N)
    else:
        previous_xlims = ax_wave.get_xlim()

    try:
        print("Plotting waveform of index: {:d}".format(waveform_index))

        fig.suptitle('Waveform, index: {:d}, ch: {:d}, timestamp: {:d},\nfile: {}'.format(
                     waveform_index,
                     waveform["channel"],
                     waveform["timestamp"],
                     args.file_name))
        ax_wave.clear()
        ax_wave.step(range(N), 
                     waveform["samples"],
                     color = "C0",
                     where = 'post')
        ax_wave.set_xlim(previous_xlims[0], previous_xlims[1])
        ax_wave.set_yscale('linear')
        ax_wave.grid()

        ax_gate.clear()

        for index, gate in enumerate(waveform["gates"]):
            print("Plotting gate of index: {:d}.{:d}".format(waveform_index, index))
            ax_gate.step(range(N),
                         gate,
                         label = "Gate {:d}".format(index),
                         color = "C{:d}".format((index + 1) % 10),
                         where = 'post')
        ax_gate.set_xlim(previous_xlims[0], previous_xlims[1])
        ax_gate.grid()
        ax_gate.legend()

        canvas.draw()
    except Exception as e:
        print("ERROR: Unable to plot the new waveform: {}".format(e))

def plot_transform(waveform):
    global waveform_index
    global args
    global previous_xlims
    global canvas
    global ax_wave
    global ax_gate

    average = np.average(waveform["samples"][0:args.baseline_samples])

    N = len(waveform["samples"])
    #transform = fft.rfft(waveform["samples"] - average)
    transform = fft.rfft(15600 - np.asarray(waveform["samples"]))

    magnitudes = np.abs(transform)
    phases = np.angle(transform)

    M = len(magnitudes)
    T = N * args.clock_step * 1e-9

    frequencies = fft.rfftfreq(N, d = args.clock_step * 1e-9) * 1e-6

    magnitudes[0] = 0

    if previous_xlims == None:
        previous_xlims = (0, max(frequencies))
    else:
        previous_xlims = ax_wave.get_xlim()

    try:
        print("Plotting transform of index: {:d}".format(waveform_index))

        fig.suptitle('Fourier transform, index: {:d}, ch: {:d}, timestamp: {:d},\nfile: {}'.format(
                     waveform_index,
                     waveform["channel"],
                     waveform["timestamp"],
                     args.file_name))

        ax_wave.clear()
        ax_wave.step(frequencies,
                     magnitudes,
                     color = "C0",
                     where = 'post')
        ax_wave.set_xlim(previous_xlims[0], previous_xlims[1])
        ax_wave.set_yscale('log')
        ax_wave.grid()
        ax_wave.set_xlabel("Frequency [MHz]")

        ax_gate.clear()

        ax_gate.step(frequencies,
                     phases,
                     color = "C1",
                     where = 'post')
        ax_gate.set_xlim(previous_xlims[0], previous_xlims[1])
        ax_gate.grid()
        ax_gate.set_xlabel("Frequency [MHz]")

        canvas.draw()
    except Exception as e:
        print("ERROR: Unable to plot the new transform: {}".format(e))
        
def export_waveform(waveform):
    global args

    try:
        print("Exporting waveform of index: {:d}".format(waveform_index))

        basename, extension = os.path.splitext(args.file_name)

        new_file = "{}_wave_index{:d}_ch{:d}_timestamp{:d}.csv".format(
                   basename,
                   waveform_index,
                   waveform["channel"],
                   waveform["timestamp"])

        print("Exporting to {}".format(new_file))
                     
        with open(new_file, "w") as output_file:
            output_file.write("#i\tsample")

            for index, gate in enumerate(waveform["gates"]):
                output_file.write("\tgate{:d}".format(index))

            output_file.write("\n")

            for index, sample in enumerate(waveform["samples"]):
                output_file.write("{:d}\t{:d}".format(index, sample))

                for gate in waveform["gates"]:
                    output_file.write("\t{:d}".format(gate[index]))

                output_file.write("\n")

    except Exception as e:
        print("ERROR: Unable to export the new waveform: {}".format(e))

def quit_gui():
    print("Quitting")

    # Closes the input file
    input_file.close()

    # Stops main loop
    root.quit()
    # This is necessary on Windows to prevent:
    # Fatal Python Error: PyEval_RestoreThread: NULL tstate
    root.destroy()

def on_key_press(event):
    global show_transform
    global previous_xlims

    print("Pressed key: '{}'".format(event.key))
    # Disabling the default key_press events so it does not interfere with the
    # waveforms navigation
    #key_press_handler(event, canvas, toolbar)

    if event.key == 'h':
        print("Resetting view")
        previous_xlims = None
        plot_update(curr_waveform())

    elif event.key == 'f':
        # Toggle transform display
        if show_transform:
            show_transform = False
        else:
            show_transform = True

        previous_xlims = None
        print("Calculating Fourier transform")
        plot_update(curr_waveform())

    elif event.key == 'e':
        print("Exporting waveform")
        export_waveform(curr_waveform())

    elif event.key == 'right':
        print("Loading next waveform")
        plot_update(next_waveform())

    elif event.key == 'left':
        print("Loading previous waveform")
        plot_update(prev_waveform())

    elif event.key == 'down':
        print("Loading next waveform, jump ahead of 10 waveforms")
        for i in range(10 - 1):
            next_waveform()
        plot_update(next_waveform())

    elif event.key == 'up':
        print("Loading previous waveform, jump after of 10 waveforms")
        for i in range(10 - 1):
            prev_waveform()
        plot_update(prev_waveform())

    elif event.key == 'pagedown':
        print("Loading next waveform, jump ahead of 100 waveforms")
        for i in range(100 - 1):
            next_waveform()
        plot_update(next_waveform())

    elif event.key == 'pageup':
        print("Loading previous waveform, jump after of 100 waveforms")
        for i in range(100 - 1):
            prev_waveform()
        plot_update(prev_waveform())

    elif event.key == 'q':
        quit_gui()

root = tkinter.Tk()

root.wm_title("ABCD waveforms display")

fig = Figure(figsize=(8, 4.5))#, dpi=120)
ax_wave = fig.add_subplot(211)
ax_gate = fig.add_subplot(212, sharex = ax_wave)

canvas = FigureCanvasTkAgg(fig, master = root)
canvas.draw()
canvas.get_tk_widget().pack(side=tkinter.TOP, fill=tkinter.BOTH, expand=1)

#NavigationToolbar2Tk.toolitems = [toolitem for toolitem in NavigationToolbar2Tk.toolitems if not toolitem[0] == 'Home']

# This is the standard Matplotlib toolbar
toolbar = NavigationToolbar2Tk(canvas, root)
toolbar.update()

canvas.get_tk_widget().pack(side=tkinter.TOP, fill=tkinter.BOTH, expand=1)

canvas.mpl_connect("key_press_event", on_key_press)

print("Loading first waveform")
plot_update(next_waveform())
tkinter.mainloop()

# If you put root.destroy() here, it will cause an error if the window is
# closed with the window manager.
