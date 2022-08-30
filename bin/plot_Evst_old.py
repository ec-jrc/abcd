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

import argparse
import struct
import os
import json

import math

import numpy as np
from scipy import optimize
import matplotlib.pyplot as plt
from matplotlib.colors import LogNorm

NS_PER_SAMPLE = 2.0 / 1024
TIME_RESOLUTION = 0.2
ENERGY_RESOLUTION = 20
ENERGY_MIN = 400
ENERGY_MAX = 20000
TIME_MIN = -1
TIME_MAX = -1
EVENTS_COUNT = -1

MIN_T_FIT = 3 # In units of tau
MAX_T_FIT = 6 # In units of tau
FIT_REPETITIONS = 4

def gaussian(p, x):
    A, mu, sigma = p
    return A * np.exp(-0.5 * (x - mu) * (x - mu) / (sigma * sigma))

def exponential(p, x):
    A, tau = p
    return A * np.exp(-x / tau)

def residuals(p, y, x):
    return y - exponential(p, x)

def running_mean(x, N):
    Np = (N // 2) * 2 + 1
    print("N: {:d}, N': {:d}".format(N, Np))

    padded_samples = np.insert(x, 0, np.zeros((N // 2 + 1, )))
    padded_samples = np.append(padded_samples, np.zeros((N // 2, )))

    #cumsum = np.cumsum(np.insert(x, 0, np.zeros((N // 2, ))) 
    cumsum = np.cumsum(padded_samples)
    return (cumsum[Np:] - cumsum[:-Np]) / Np 

parser = argparse.ArgumentParser(description='Read and print ABCD data files')
parser.add_argument('file_name',
                    type = str,
                    help = 'Input file name')
parser.add_argument('channel',
                    type = str,
                    help = 'Channel selection (all or number)')
parser.add_argument('-l',
                    '--logscale',
                    action = "store_true",
                    help = 'Display spectra in logscale')
parser.add_argument('-n',
                    '--ns_per_sample',
                    type = float,
                    default = NS_PER_SAMPLE,
                    help = 'Nanoseconds per sample (default: {:f})'.format(NS_PER_SAMPLE))
parser.add_argument('-N',
                    '--events_count',
                    type = int,
                    default = EVENTS_COUNT,
                    help = 'Events to be read from file (default: {:f})'.format(EVENTS_COUNT))
parser.add_argument('-r',
                    '--time_resolution',
                    type = float,
                    default = TIME_RESOLUTION,
                    help = 'Time resolution (default: {:f})'.format(TIME_RESOLUTION))
parser.add_argument('-t',
                    '--time_min',
                    type = float,
                    default = TIME_MIN,
                    help = 'Time min (default: {:f})'.format(TIME_MIN))
parser.add_argument('-T',
                    '--time_max',
                    type = float,
                    default = TIME_MAX,
                    help = 'Time max (default: {:f})'.format(TIME_MAX))
parser.add_argument('-R',
                    '--energy_resolution',
                    type = float,
                    default = ENERGY_RESOLUTION,
                    help = 'Energy resolution (default: {:f})'.format(ENERGY_RESOLUTION))
parser.add_argument('-e',
                    '--energy_min',
                    type = float,
                    default = ENERGY_MIN,
                    help = 'Energy min (default: {:f})'.format(ENERGY_MIN))
parser.add_argument('-E',
                    '--energy_max',
                    type = float,
                    default = ENERGY_MAX,
                    help = 'Energy max (default: {:f})'.format(ENERGY_MAX))
parser.add_argument('-s',
                    '--save_data',
                    type = str,
                    default = None,
                    help = 'Save histograms to file')

args = parser.parse_args()

# One more H than expected, I guess the compiler is padding the data
event_PSD_struct = struct.Struct("<QHHHBB")

event_PSD_dtype = np.dtype([('timetag', np.uint64),
                            ('qshort', np.uint16),
                            ('qlong', np.uint16),
                            ('baseline', np.uint16),
                            ('channel', np.uint8),
                            ('pur', np.uint8),
                            ])
                            
print("### ### Reading: {}".format(args.file_name))
data = np.fromfile(args.file_name, dtype = event_PSD_dtype, count = args.events_count) #26905012)

try:
    channel = int(args.channel)
    channel_selection = data['channel'] == channel

except ValueError:
    channel_selection = data['channel'] >= 0
    
#selection = data['channel'] >= 0
#selection = np.logical_and(data['channel'] == 2, np.logical_and(1500e9 < data['timetag'], data['timetag'] < 1700e9))
#qshorts = data['qshort'][selection]
qlongs = data['qlong'][channel_selection]
timetags = data['timetag'][channel_selection] * 1e-9 * args.ns_per_sample
purs = data['pur'][channel_selection]

print("Timetags length:", len(timetags))

if args.time_min > 0:
    min_time = args.time_min
else:
    min_time = min(timetags)

if args.time_max > 0:
    max_time = args.time_max
else:
    max_time = max(timetags)

Delta_time = max_time - min_time
time_resolution = args.time_resolution
N_t = math.floor((Delta_time) / time_resolution)

min_energy = args.energy_min
max_energy = args.energy_max
energy_resolution = args.energy_resolution
N_E = math.floor((max_energy - min_energy) / energy_resolution)
Delta_bins_E = (max_energy - min_energy) / N_E

max_bin_edge_energy = min_energy + energy_resolution * N_E
max_bin_edge_time = min_time + time_resolution * N_t
Delta_bins_time = time_resolution * N_t

print("Time: min: {}, max: {}, max_bin_edge: {}, Delta: {}, Delta bins: {}".format(min_time, max_time, max_bin_edge_time, Delta_time, Delta_bins_time))
print("Energy: min: {:f}, max: {:f}".format(min_energy, max_energy))
print("N_t: {:d}, N_E: {:d}".format(N_t, N_E))

histo2d, energy_edges, time_edges = np.histogram2d(qlongs, timetags, bins = (N_E, N_t), range = ((min_energy, max_bin_edge_energy), (min_time, max_bin_edge_time)))

#print("Time edges:", time_edges)

energy_selection = np.logical_and(min_energy < qlongs, qlongs < max_energy)
time_selection = np.logical_and(min_time < timetags, timetags < max_bin_edge_time)
total_selection = np.logical_and(energy_selection, time_selection)

mean_rates, bin_edges = np.histogram(timetags[total_selection], bins = time_edges)
mean_rates = mean_rates / time_resolution / 1000

rates_histo, rates_edges = np.histogram(mean_rates, bins = 100)

spec_histo, bin_edges = np.histogram(qlongs[total_selection], bins = energy_edges)

spec_smoothed = running_mean(spec_histo, 10)

spec_derivative = -1 * np.gradient(spec_smoothed, Delta_bins_E)


left_timetags = timetags[total_selection][:-1]
right_timetags = timetags[total_selection][1:]

Delta_t = right_timetags - left_timetags
rates = 1 / Delta_t

Delta_t_histo, Delta_t_bin_edges = np.histogram(Delta_t, bins = 500, range = (0, Delta_t.max()))

# Guess the initial value of tau using the exponential integral
# I = A \int_0^\infty \text{e}^{-x / \tau} = A \tau
A = Delta_t_histo.max()
I = np.trapz(Delta_t_histo, x = Delta_t_bin_edges[:-1])
tau = I / A

p0 = (A, tau)
print("p0: A: {:f}, I: {:f}, tau: {:g}".format(A, I, tau))

try:
    for fit_index in range(FIT_REPETITIONS):

        # Selection of the data
        fit_selection = np.logical_and((MIN_T_FIT * tau) < Delta_t_bin_edges[:-1], Delta_t_bin_edges[:-1] < (MAX_T_FIT * tau))

        # Do the fit!
        full_output = optimize.leastsq(residuals,
                                    p0,
                                    args = (Delta_t_histo[fit_selection], Delta_t_bin_edges[:-1][fit_selection]),
                                    full_output = True)

        p = full_output[0]
        A, tau = p

        print("fit_index: {:d}; p : A: {:f}, I: {:f}, tau: {:g}, real rate: {:g}".format(fit_index, A, I, tau, 1 / tau))

        # Update the fit selection
        fit_selection = np.logical_and((MIN_T_FIT * tau) < Delta_t_bin_edges[:-1], Delta_t_bin_edges[:-1] < (MAX_T_FIT * tau))

        p0 = (A, tau)
except:
    pass

Delta_t_histo2, Delta_t_bin_edges2 = np.histogram(Delta_t, bins = 500, range = (0, tau * MIN_T_FIT))

print("Data len: {:d}, last timetag: {:f}, average rate: {:f}, min(Delta_t): {:f}, max(rates): {:f}".format(len(timetags), timetags[-1], mean_rates.mean(), min(Delta_t), max(rates) / 1000))

true_counting_rate = 1. / tau
#measured_counting_rate = mean_rates.mean() * 1000
measured_counting_rate = np.sum(total_selection) / Delta_bins_time

dead_time = (true_counting_rate - measured_counting_rate) / (true_counting_rate * measured_counting_rate)

print("true counting rate: {:g}, measured counting rate: {:g}, dead time: {:g}".format(true_counting_rate, measured_counting_rate, dead_time * 1e3))

output = {"file_name": args.file_name,
          "measured_rate": measured_counting_rate,
          "true_rate": true_counting_rate,
          "dead_time": dead_time,
          "Delta_t": Delta_bins_time,
         }

print(json.dumps(output))

if args.save_data is not None:
    basename, extension = os.path.splitext(args.save_data)

    print(energy_edges.shape, spec_histo.shape)

    output_file_name = basename + '_qlong' + extension
    output_array = np.vstack((energy_edges[:-1], spec_histo)).T

    print("Writing qlong histogram to: {}".format(output_file_name))
    np.savetxt(output_file_name, output_array)
    
    output_file_name = basename + '_deriv' + extension
    output_array = np.vstack((energy_edges[:-1], spec_derivative)).T

    print("Writing derivative of qlong histogram to: {}".format(output_file_name))
    np.savetxt(output_file_name, output_array)
    
    output_file_name = basename + '_rate' + extension
    output_array = np.vstack((time_edges[:-1], mean_rates)).T

    print("Writing rates to: {}".format(output_file_name))
    np.savetxt(output_file_name, output_array)
else:
    #plt.ion()

    fig = plt.figure()

    spect_ax = fig.add_subplot(211)

    spect_ax.plot(energy_edges[:-1], spec_smoothed)
    spect_ax.set_xlim(min_energy, max_energy)
    if args.logscale:
        spect_ax.set_yscale('log')
    spect_ax.set_xlabel('Energy [ch]')
    spect_ax.grid()

    spect_ax.set_title('Smoothed spectrum')

    deriv_ax = fig.add_subplot(212, sharex = spect_ax)

    deriv_ax.plot(energy_edges[:-1] + Delta_bins_E / 2.0, spec_derivative)
    deriv_ax.set_xlim(min_energy, max_energy)
    deriv_ax.set_xlabel('Energy [ch]')
    deriv_ax.grid()

    deriv_ax.set_title('Spectrum derivative')

    fig = plt.figure()

    histo_ax = fig.add_subplot(222)

    histo_ax.plot(energy_edges[:-1], spec_histo)
    histo_ax.set_xlim(min_energy, max_energy)
    if args.logscale:
        histo_ax.set_yscale('log')
    histo_ax.set_xlabel('Energy [ch]')
    histo_ax.grid()

    bihisto_ax = fig.add_subplot(221)

    bihisto_ax.imshow(histo2d,
          origin = 'lower',
          norm = LogNorm(),
          interpolation = 'none',
          extent = (min(time_edges), max(time_edges),
                    min(energy_edges), max(energy_edges)),
          aspect = 'auto')

    #bihisto_ax.set_yscale('log')
    bihisto_ax.set_ylabel('Energy [ch]')
    bihisto_ax.set_xlabel('Time [s]')

    bihisto_ax.grid()

    rate_ax = fig.add_subplot(223, sharex = bihisto_ax)

    rate_ax.step(time_edges[:-1], mean_rates)
    rate_ax.set_xlim(min_time, max_bin_edge_time)
    rate_ax.set_ylabel("Rate [kHz]")
    rate_ax.set_xlabel('Time [s]')
    rate_ax.grid()

    rateh_ax = fig.add_subplot(224)

    rateh_ax.step(rates_edges[:-1], rates_histo)
    rateh_ax.set_xlabel("Rate [kHz]")
    rateh_ax.grid()

    fig = plt.figure()

    ax = fig.add_subplot(211)

    ax.step(Delta_t_bin_edges[:-1], Delta_t_histo, where = 'mid', label = 'Δt histogram', color = 'green')
    #ax.step(Delta_t_bin_edges[:-1][fit_selection], Delta_t_histo[fit_selection], where = 'mid')
    ax.fill_between(Delta_t_bin_edges[:-1][fit_selection], 0, Delta_t_histo[fit_selection], label = 'Fitting area', facecolor='green', alpha=0.2)
    #ax.plot([MIN_T_FIT * tau, MIN_T_FIT * tau, MAX_T_FIT * tau, MAX_T_FIT * tau], [1, Delta_t_histo.max() / 2, Delta_t_histo.max() / 2, 1])

    Delta_t_linspace = np.linspace(Delta_t_bin_edges[0], Delta_t_bin_edges[-1], (len(Delta_t_bin_edges) - 1) * 2)
    ax.plot(Delta_t_linspace, exponential(p, Delta_t_linspace), label = 'Fit', color = 'black')

    if args.logscale:
        ax.set_yscale('log')
    ax.grid()
    ax.legend()

    ax.set_title('Δt histogram (τ: {:g})'.format(tau))

    ax = fig.add_subplot(212)

    ax.step(Delta_t_bin_edges2[:-1], Delta_t_histo2, where = 'mid', color = 'green')

    Delta_t_linspace = np.linspace(Delta_t_bin_edges2[0], Delta_t_bin_edges2[-1], (len(Delta_t_bin_edges) - 1) * 2)
    jacobian = (Delta_t_bin_edges2[-1] - Delta_t_bin_edges2[0]) / (Delta_t_bin_edges[-1] - Delta_t_bin_edges[0])
    ax.plot(Delta_t_linspace, exponential(p, Delta_t_linspace) * jacobian, label = 'Fit', color = 'black')


    if args.logscale:
        ax.set_yscale('log')
    ax.grid()

    ax.set_title('Δt histogram (up to {:d}τ)'.format(MIN_T_FIT))

    try:
        fig = plt.figure()

        ax = fig.add_subplot(111)

        ax.plot(left_timetags)

        ax.set_title("Timetags")
        ax.grid()

    #    ax = fig.add_subplot(212)
    #
    #    ax.set_title("PUR")
    #    ax.plot(purs)
    #
    #    ax.grid()
    #
    #    fig = plt.figure()
    #
    #    ax = fig.add_subplot(211)
    #
    #    ax.step(left_timetags, Delta_t)
    #
    #    ax.set_ylabel("$\\Delta t$ [s]")
    #
    #    ax = fig.add_subplot(212, sharex = ax)
    #    ax = fig.add_subplot(212)
    
    #    ax.step(left_timetags, rates / 1000)
    
    #    ax.set_ylabel("$1/\\Delta t$ [kHz]")
    except MemoryError:
        print("Too many points to plot")
    
    plt.show()
