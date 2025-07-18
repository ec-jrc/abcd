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
cmake --build build --parallel 8
sudo cmake --install build
```

If doing a manual instal with `cmake --install ...` then `ldconfig` should be launched after the installation.
`ldconfig` configures the cache of the system-wide libraries, so that the waan libraries can be located by `waan`.

Modules interfacing with hardware are not built by default, as they depend on specific vendors' libraries.
To compile the specific modules use the `-D BUILD_<MODULE>=ON` option in the configuration phase, as in:

```
cmake -S . -B build -D BUILD_ABSP=ON -D BUILD_ABCD=ON
cmake --build build --parallel 8
sudo cmake --install build
```

## Ubuntu package(s) generation

An Ubuntu package may be generated with CMake and CPack.
The default prefix for CMake on UNIX platforms is `/usr/local`, but if should be changed to `/usr` at the configuration phase to generate a proper `deb` package.
Some scripts need to be configured with the proper prefix and this is done at the configuration phase.
There is no need to run `ldconfig` after the installation, as it is automatically run by the `postinst` script of the `deb` package.

To generate and install the `deb` package:

```
cmake -S . -B build --install-prefix /usr
cmake --build build --parallel 8
cd build/
cpack
sudo dpkg -i abcd-core-...
```

Hardware interfacing modules need to be explicitly enabled as in the standard build.
If enabled, separate packages are generated for each hardware module, as in:

```
cmake -S . -B build -D BUILD_ABSP=ON -D BUILD_ABCD=ON --install-prefix /usr
cmake --build build --parallel 8
cd build/
cpack
sudo dpkg -i abcd-core-...
sudo dpkg -i abcd-absp-...
sudo dpkg -i abcd-abcd-...
```
