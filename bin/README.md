# ABCD utility scripts

## Plotting results
There are some python 3 scripts that can be used to display some information in the events files.
They depend on Numpy, Scipy and Matplotlib.

* [`plot_spectra.py`](./plot_spectra.py) displays the time-normalized energy spectra of several data files and it can save the spectra to a CSV file;
* [`plot_PSD.py`](./plot_PSD.py) displays the PSD bidimensional histogram and it can save the PSD spectra to a CSV file;
* [`plot_ToF.py`](./plot_ToF.py) displays the Time of Flight between two channels;
* [`plot_Evst.py`](./plot_Evst.py) displays the energy spectrum and its evolution in time. It also plots the evolution of the data rate in time. This is very useful to diagnose if the DAQ has some dead time.

All the scripts have an in-line help, just call them with the `-h` flag, _e.g._:

    acq-rfx@neutron-rfx:~$ ~/abcd/bin/plot_Evst.py -h
    usage: plot_Evst.py [-h] [-l] [-n NS_PER_SAMPLE] [-N EVENTS_COUNT]
                        [-r TIME_RESOLUTION] [-t TIME_MIN] [-T TIME_MAX]
                        [-R ENERGY_RESOLUTION] [-e ENERGY_MIN] [-E ENERGY_MAX]
                        [-s SAVE_DATA]
                        file_name channel

    Read and print ABCD data files

    positional arguments:
      file_name             Input file name
      channel               Channel selection (all or number)

    optional arguments:
      -h, --help            show this help message and exit
      -l, --logscale        Display spectra in logscale
      -n NS_PER_SAMPLE, --ns_per_sample NS_PER_SAMPLE
                            Nanoseconds per sample (default: 0.001953)
      -N EVENTS_COUNT, --events_count EVENTS_COUNT
                            Events to be read from file (default: -1.000000)
      -r TIME_RESOLUTION, --time_resolution TIME_RESOLUTION
                            Time resolution (default: 0.200000)
      -t TIME_MIN, --time_min TIME_MIN
                            Time min (default: -1.000000)
      -T TIME_MAX, --time_max TIME_MAX
                            Time max (default: -1.000000)
      -R ENERGY_RESOLUTION, --energy_resolution ENERGY_RESOLUTION
                            Energy resolution (default: 20.000000)
      -e ENERGY_MIN, --energy_min ENERGY_MIN
                            Energy min (default: 400.000000)
      -E ENERGY_MAX, --energy_max ENERGY_MAX
                            Energy max (default: 20000.000000)
      -s SAVE_DATA, --save_data SAVE_DATA
                            Save histograms to file


## Events files management
The [`split_events_files.py`](./split_events_files.py) can cut an `ade` file in chunks on the basis of the timestamps.
It can be used to select a temporal subrange of the input `ade` and split it in data files with uniform temporal lengths.
