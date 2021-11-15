# ABCD: Acquisition and Broadcast of Collected Data

ABCD is a distributed Data Acquisition System (DAQ), in which each task related to the DAQ runs in a separate process (_e.g._ data acquisition, hardware management, data processing, analysis, HV management, ...).
Its main application is the acquisition of data from detectors in Nuclear Physics experiments.
The system is composed of a set of very simple modules that exchange information through dedicated communication sockets.
ABCD supports the acquisition from various signal digitizers from different vendors (_e.g._ [CAEN](http://www.caen.it/), [Red Pitaya](https://www.redpitaya.com/), and [Digilent](https://store.digilentinc.com/)).
The user interface is implemented as a web-service and can be accessed with a regular web browser.

## Introductory video
[![ABCD introduction](http://img.youtube.com/vi/6cBJLRP9S2I/0.jpg)](http://www.youtube.com/watch?v=6cBJLRP9S2I "ABCD introduction")

## Table of Contents

* [Getting ready](#getting-ready)
    * [Requirements](#requirements)
    * [Installation](#installation)
    * [Compilation](#compilation)
    * [Kernel updates](#kernel-updates)
* [Running the system](#running-the-system)
    * [Interacting with the system](#interacting-with-the-system)
    * [DAQ events logging](#daq-events-logging)
* [Modules](#modules)
* [Data streams](#data-streams)
    * [PSD events binary protocols](#psd-events-binary-protocols)
    * [Waveforms binary protocols](#waveforms-binary-protocols)
* [Saving files](#saving-files)
    * [File conversion](#file-conversion)
    * [Plotting results](#plotting-results)
* [Interfacing](#interfacing)
* [Changelog](#changelog)
* [TODO](#todo)
* [Acknowledgments](#acknowledgments)

## Getting ready
### Requirements
ABCD has being used in various linux distributions (_e.g._ Centos 7 and 8, Fedora 23 and older, Debian 9, and Ubuntu 18.04 LTS and older).
We suggest to have installed:

* [git](https://git-scm.com/) (for fetching the updated source code)
* [Linux Standard Base (LSB) utilities](https://en.wikipedia.org/wiki/Linux_Standard_Base) (used only in the installer script to detect the Linux distribution)

If the installer script (see the [following paragraph](#installation)) does not work, the list of required libraries and tools is:

* tmux;
* clang or gcc;
* ZeroMQ messaging library;
* GNU Scientific Library;
* JsonCpp;
* Jansson;
* Python3;
* Python3 support for ZeroMQ;
* NumPy, SciPy and Matplotlib for python3;
* Node.js 10 or later;
* NPM;
* zlib and libbzip2;
* kernel sources to install CAEN libraries.

For the installation details of the hardware, you will have to refer to the vendors' websites.
For the support of the CAEN Digitizers, the following CAEN libraries are required:

* [CAEN VMELib](https://www.caen.it/products/caenvmelib-library/)
* [CAEN Comm](https://www.caen.it/products/caencomm-library/)
* [CAEN Digitizer](https://www.caen.it/products/caendigitizer-library/)
* [CAEN DPP](https://www.caen.it/products/caendpp-library/)
* CAEN USB driver

For the support of the Digilent Analog Discovery 2 (AD2), the requirements are:

* [WaveForms](https://store.digilentinc.com/digilent-waveforms/) software for its SDK
* [Adept 2](https://reference.digilentinc.com/reference/software/adept/) for the hardware communication

The Red Pitaya STEMlab has the libraries already installed, refer to the [documentation](https://redpitaya.readthedocs.io/en/latest/).

### Installation
You can download directly the repository from GitHub using git:

    git clone https://github.com/ec-jrc/abcd.git

A bash [installation script](./install_dependencies.sh) is available:

    bash install_dependencies.sh

The script tries to identify the Linux distribution, if it detects Fedora, Ubuntu or Debian it is able to update the distribution and install the required packages.
If necessary, it will download and execute the installer for Node.js 10, from the Node.js official web page.

The Graphical User Interface (GUI) modules must be installed independently:

    cd wit/
    npm install

These will download all the required packages for the Node.js server and install them locally.
Once all the packages have been installed the system is ready to be run from the local folder.

### Compilation
The default compiler is `clang`. It is possible to change the compiler to `gcc` modifying the file [`common_definitions.mk`](./common_definitions.mk).
The global Makefile compiles the whole system just by running in the global directory:

    make

The hardware interfacing modules will not be compiled, as they depend on specific libraries that might not be installed.
The user should compile the suitable modules for the hardware (_e.g._ `abcd`, `abad2`, `abps5000a`, ` abrp`, or ` hivo`).

Update the location in the startup scripts.

### Kernel updates
It seems that the CAEN USB kernel module needs to be recompiled every time that the kernel is updated.
Therefore if the system updates the kernel, we suggest to reinstall the CAEN libraries.

## Running the system
The system is designed to be always running in the computer and be remotely controlled.
There are example startup scripts in the folder [`startup`](./startup).
It is possible to run the startup script at the boot of the computer, without having to log in as a user.
[tmux](http://tmux.github.io/) is employed in order to run these processes in the background.
tmux can create virtual terminals that are not attached to a particular graphical window or terminal.
The processes can run in a background session of tmux, but the user can still log into that session, check the running status, and then detach from the session without ever stopping the processes.
In the startup script the system is started in a tmux session called `ABCD`.

### Interacting with the system
A GUI was implemented as a web-service using a Node.js server.
The web server is started in the [`startup_example.sh`](./startup/startup_example.sh) script as well.
With the default setting the interface can be accessed with the link

> [http://localhost:8080](http://localhost:8080)

There are also a few utilities written in python that give the possibility to interact from the command line.
For instance [`send_command.py`](./bin/send_command.py) can be used to send a command to a process that supports commands.
[`stop_ABCD.sh`](./startup/stop_ABCD.sh) is a script that can stop the execution of a running instance of ABCD.

### DAQ events logging
In the default startup script some events loggers are activated.
They are python scripts that log the DAQ events (such as reconfigurations, start, stops...) and are saved in the [log/](./log/) directory, as text files.

## Modules
The list of all the servers available so far follows.

* [abcd](./abcd/) - Data acquisition module, that interfaces with the CAEN digitizers.
* [abad2](./abad2/) - Data acquisition module, that interfaces with the Digilent Analog Discovery 2.
* [abps5000a](./abps5000a/) - Data acquisition module, that interfaces with the Picoscope 5000A.
* [abrp](./abrp/) - Data acquisition module, that interfaces with the Red Pitaya.
* [efg](./efg/) - **Deprecated** Graphical user interface module, that hosts the web-service.
* [web_interface](./web_interface/) - **Deprecated** Second graphical user interface module, that hosts the web-service for tofcalc and spec.
* [wit](./wit/) - New Graphical user interface module, that hosts the web-service.
* [hivo](./hivo/) - High-voltages management module, that interfaces with CAEN high-voltage power supplies.
* [dasa](./dasa/) - Data logging module, that saves data to disk.
* [spec](./spec/) - Histogramming module, that calculates the energy histograms on-line.
* [tofcalc](./tofcalc/) - On-line Time of Flight calculation between some reference channels and the others.
* [waan](./waan/) - General purpose waveforms processing module (that replaces waps and waph).
* [fifo](./fifo/) - Temporary data storage with a FIFO structure, data are kept in memory for a given time span and then are discarded.
* [chafi](./chafi/) - Channel filter, that filters the waveforms and DPP events allowing the passage of only the data coming from a specific channel.
* [cofi](./cofi/) - Coincidence filter, that filters the DPP events allowing the passage of only the data in coincidence in a specified time-window.
* [enfi](./enfi/) - Energy filter that filters the DPP events allowing the passage of only the data with an energy in a specified interval.
* [read_status](./read_status/) - Example program in C99 that reads from the status socket.
* [replay_raw](./replay/) - System simulator, that reads the raw data generated by `dasa` and replays it simulating a full working system.
* [replay_events](./replay/) - System simulator, that reads the events data generated by `dasa` and replays it simulating a working system, but without the status information.
* [wafi](./wafi/) - Waveforms filter, that reads the waveforms data streams and formats it for visualization purposes for the GUI.
* [gzad](./gzad/) - Compress data, filter that compresses with the zlib or the bz2 libraries the messages in a socket stream (compression ratio about 50%).
* [unzad](./gzad/) - Decompress data, filter that decompresses the packets generated by the `gzad` filter.
* [pufi](./pufi/) - Pulse Shape Discrimination filter, that selects data that fall in a polygon on the (energy, PSD) plane.
* [waps](./waps/) - Waveforms to PSD events, data stream converter from waveforms to PSD parameters (_i.e_ short and long charge integrals).
* [waph](./waph/) - Waveforms to PHA events, data stream converter from waveforms to PHA events (_i.e._ pulse height).
* [adr2ade](./convert/) - Conversion tool from adr files to ade file.
* [ade2ascii](./convert/) - Conversion tool from ade files to ascii files.

Most of the servers are implemented following the finite state machines paradigm.
Diagrams of the state machines can be found in the [`doc/`](./doc/) folder in the repository.
Filters are usually implemented as simple loops reading from the sockets and performing their functions.

All the programs can be run in the terminal and they have an in-line help.
Call the program with a `-h` flag, _e.g._

    user@daq:~/abcd$ ./abcd/abcd -h
    =============================================
     A.B.C.D. software - v. 0.1                  
     Acquisition and Broadcast of Collected Data 
    =============================================
    
    Usage: ./abcd [options]
        -h: Display this message
        -S <address>: Status socket address, default: tcp://*:16180
        -D <address>: Data socket address, default: tcp://*:16181
        -C <address>: Commands socket address, default: tcp://*:16182
        -f <file_name>: Digitizer configuration file, default: verdi.json
        -T <period>: Set base period in milliseconds, default: 10
        -c <connection_type>: Connection type, default: 0
        -l <link_number>: Link number, default: 1
        -n <CONET_node>: Connection node, default: 0
        -V <VME_address>: VME address, default: 0
        -B <size>: Events buffer maximum size, default: 4096
        -v: Set verbose execution

## Data streams
Data is transferred using the [ZeroMQ](http://zeromq.org/) messaging library.
Since it is an agnostic mean of transportation, we implemented the necessary serialization protocols.
We opted to use PUB-SUB sockets for the data streams and statuses streams, and PUSH-PULL sockets for commands streams.

* [JSON](http://www.json.org/) is used to deliver high-level data, such as commands and statuses.
* Custom binary formats are used for the digitizers data streams.

For the moment there are only two kinds of binary data streams.

```
########################################################
# ASCIIart visual representation of the binary formats #
########################################################

+-------------------------------------------------------------------------------+
| PSD event: word of 16 bytes                                                   |
+---------------------------------------+---------+---------+---------+----+----+
| Timestamp                             |Q short  |Q long   |Baseline |Ch. |PUR |
| 64 bit                                |16 bit   |16 bit   |16 bit   |8bit|8bit|
| C99 stdint: uint64_t                  |uint16_t |uint16_t |uint16_t |uint|uint|
+---------------------------------------+---------+---------+---------+----+----+

+---------------------------------------------------------------------+
| Waveform: header of 14 bytes followed by variable length arrays     |
+---------------------------------------+----+-------------------+----+
| Timestamp                             |Ch. |Samples number (N) |M   |
| 64 bit                                |8bit|32 bit             |8bit|
| uint64_t                              |uint|uint32_t           |uint|
+---------------------------------------+----+-------------------+----+
| Samples: array of N unsigned ints, total: 16 bit x N                |
+---------+---------+---------+---------+---------+-------+-----------+
|sample[0]|sample[1]|sample[2]|sample[3]|sample[4]|       |sample[N-1]|
|16 bit   |16 bit   |16 bit   |16 bit   |16 bit   |  ...  |16 bit     |
|uint16_t |uint16_t |uint16_t |uint16_t |uint16_t |       |uint16_t   |
+---------+---------+---------+---------+---------+-------+-----------+
| Digitizer gates or additional waveforms                             |
| M arrays of N unsigned ints, total: 8 bit x M x N                   |
+-----------+----+----+----+----+------+------------------------------+
|   Gate 0: |a[0]|a[1]|a[2]|    |a[N-1]|
|           |8bit|8bit|8bit|... |8 bit |
|           |uint|uint|uint|    |uint  |
+-----------+----+----+----+----+------+
|   Gate 1: |b[0]|b[1]|b[2]|    |b[N-1]|
|           |8bit|8bit|8bit|... |8 bit |
|           |uint|uint|uint|    |uint  |
+-----------+----+----+----+----+------+
| ...                                  |
+-----------+----+----+----+----+------+
| Gate M-1: |z[0]|z[1]|z[2]|    |z[N-1]|
|           |8bit|8bit|8bit|... |8 bit |
|           |uint|uint|uint|    |uint  |
+-----------+----+----+----+----+------+
```

### PSD events binary protocols
Each PSD event is a 16 bytes binary word with:

* Timestamp - 64 bit unsigned integer;
* Charge short - 16 bit unsigned integer;
* Charge long (energy information) - 16 bit unsigned integer;
* Baseline - 16 bit unsigned integer;
* Channel number - 8 bit unsigned integer;
* PUR flag - 8 bit unsigned integer, unused.

The PHA events use the same format but the "Charge long" contains the pulse height.

This is the format used by [dasa](./dasa/) to save data as events files.

### Waveforms binary protocols
A waveform has a 14 bytes header with:

* Timestamp - 64 bit integer;
* Channel number - 8 bit unsigned integer;
* Samples number (N) - 32 bit integer;
* Gates number (M) - 8 bit integer.

Following the header there is a binary buffer of N integers of 16 bits.
This buffer contains the digitized signals.
After the samples buffer there are M binary buffers, each with N integers of 8 bits.
These buffers are the digitizer's integration gates or additional waveforms determined by the processing modules.

This is the format used by [dasa](./dasa/) to save data as waveforms files.

## Saving files
Files can be manually saved with the ABCD formats.
Data files must be **opened before the start** of an acquisition, otherwise the data is lost.
The LMNO tab is the user interface for the data saving.
The user must specify the file name, normally files are saved in the folder:

    ~/abcd/data/
    
File types:
* **Events files** are space-efficient files with only the PSD parameters acquired by the digitizer. Refer to the [PSD events binary protocol](#psd-events-binary-protocols).
* **Waveforms files** save only the waveforms recorded by the digitizer, they tend to be very big. Refer to the [waveforms binary protocol](#waveforms-binary-protocols).
* **Raw files** save both the events and waveforms in a rather complex format. They are used with the [replay_raw](./replay) program to simulate off-line a full working system.

Only raw files support the storage of a compressed data stream by `gzad`.
The raw file then should be replayed with `replay_raw` and decompressed using `unzad`.

### Example files
In the [`data/`](./data/) folder there are some examples of ABCD data files.
The events file can be plotted with the scripts in the [`bin/`](./bin/) folder.
The raw and waveforms files have been compressed with bzip.
`replay_raw.py` also supports compressed raw files.
If python is not available, the C version of `replay_raw` may be used, but it does not support compressed raw files.

### File conversion
The [`ade2ascii.py`](./conversion/ade2ascii.py) python script and the [`ade2ascii`](./conversion/ade2ascii.c) C99 program convert the events files to an ASCII file with the format:

    #N      timestamp       qshort  qlong   channel
    0       3403941888      1532    1760    4
    1       3615693824      471     561     4
    2       4078839808      210     268     4
    3       4961184768      198     216     4
    4       6212482048      775     892     4
    ...     ...             ...     ...     ...

The [`ade2ascii.m`](./conversion/ade2ascii.m) script shows how to read the data files in Octave (and Matlab) and prints them in ASCII with the [`ade2ascii.py`](./conversion/ade2ascii.py) format.

### Plotting results
There are some python scripts that can be used to display the events files.
* [`plot_spectra.py`](./bin/plot_spectra.py) displays the time-normalized energy spectra of several data files and it can save the spectra to a CSV file.
* [`plot_PSD.py`](./bin/plot_PSD.py) displays the PSD bidimensional histogram.
* [`plot_ToF.py`](./bin/plot_PSD.py) displays the time differences histogram between channel couples.
* [`plot_Evst.py`](./bin/plot_Evst.py) displays the energy spectrum and its evolution in time. It also plots the evolution of the data rate in time. This is very useful to diagnose if the DAQ has some dead time.

These scripts require the installation of python3 along with matplotlib, numpy and scipy.
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

## Interfacing
The [`examples/`](./examples) folder contains commented examples of software that interfaces with ABCD.

## Changelog
* 2021/06/16 - `replay_raw.py` supports the replay of compressed raw files.
* 2021/06/16 - Moved all the replayers and all the converters to a single directory.
* 2021/06/09 - The project is now a project officially hosted by the Joint Research Centre of the European Commission.
* 2021/04/09 - Added a general purpose waveforms processing module.
* 2019/10/01 - Added an Octave example of how to read events files.
* 2019/09/16 - Added support for the PicoScope 5000A.
* 2019/02/06 - Added a simple C conversion program from the ade format to ASCII.
* 2018/02/08 - Added the on-line ToF calculator.
* 2018/02/08 - Added support for the DT5725 (needs more testing).
* 2018/03/27 - Added support for the Digilent Analog Discovery 2
* 2018/12/19 - Added a digital Constant Fraction Discriminator in waps, it calculates the timestamp of a waveform with a higher resolution than a single bin
* 2018/12/24 - Added an online display of the PSD bidimensional histogram, using the new `spec` module

## TODO
* Add CAEN's DPP-PHA support.
* Port the remaining software from JsonCpp to Jansson to reduce the number of dependencies.

## Acknowledgments
This DAQ was developed for the [RRTNIS subsystem](http://www.cbord-h2020.eu/page/media_items/tagged-neutron-inspection-system4.php) of the [C-BORD project](http://www.cbord-h2020.eu/).
