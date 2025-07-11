# ABCD: Acquisition and Broadcast of Collected Data

**ABCD** is a distributed Data Acquisition (DAQ) framework, in which each task related to the DAQ runs in a separate process (*e.g.* data acquisition, hardware management, data processing, analysis,...).
Its main application is the acquisition of data from signal digitizers in Nuclear Physics experiments.
The system is composed of a set of simple modules that exchange information through dedicated [communication sockets](https://en.wikipedia.org/wiki/Network_socket).
ABCD supports the acquisition from various signal digitizers from different vendors (*e.g.* [CAEN](http://www.caen.it/), [SP Devices](https://www.spdevices.com/), [Red Pitaya](https://www.redpitaya.com/), and [Digilent](https://store.digilentinc.com/)).
The user interface is implemented as a web-service and can be accessed with a regular web browser.

The official documentation and tutorial reside at: https://abcd-docs.readthedocs.io/

## Compilation

Use CMake to build and install ABCD:

```
cmake -S . -B build
cmake --build build
sudo cmake --install build
```

If manually installing, the location of the `waan` libraries must be manually configured by launching `ldconfig`.

## Ubuntu package generation

An Ubuntu package may be generated with CPack.
In this case `ldconfig` is automatically run by the `postinst` script of the `deb` package.
To generate and install the `deb` package:

```
cd build/
cpack
sudo dpkg -i abcd-code-...
```