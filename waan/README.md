# waan - WAveforms ANalysis

`waan` is a general purpose waveforms analysis software.
It allows users to define their own pulses analysis routines, without worrying about the inner workings of the ABCD data acquisition system (DAQ).
The user is responsible to write two libraries in C99, that take a waveform as input and outputs a processed event.
The user functions can be modified and reloaded at runtime, simplifying the development phase.

## Structure
The following asciiart diagram shows the structure of a pulse analysis.
```
################################################################################
#                                                                              #
#                                                    +-------------------+     #
#                                                    | +-------------------+   #
#                                                    | | +-------------------+ #
# +----------+                                       | | | user libraries    | #
# | Waveform | <== In the form of an                 +-| | (may be different | #
# +----------+     event_waveform object,              +-| for each channel) | #
#      |           see include/events.h                  +-------------------+ #
#      v                                                           |           #
# +----------------------+    +--------+    +------------------+   |           #
# | user function:       | <- | user   | <- | user function:   | <-+           #
# | timestamp_analysis() |    | config |    | timestamp_init() |   |           #
# +----------------------+    +--------+    +------------------+   |           #
#      |             |                                             |           #
#      v             v                                             |           #
# +----------+   +-------+                                         |           #
# | Waveform |   | Event | <== In the form of an                   |           #
# +----------+   +-------+     event_PSD object,                   |           #
#      |             |         see include/events.h                |           #
#      v             v                                             |           #
# +----------------------+    +--------+    +----------------+     |           #
# | user function:       | <- | user   | <- | user function: | <---+           #
# | energy_analysis()    |    | config |    | energy_init()  |                 #
# +----------------------+    +--------+    +----------------+                 #
#      |             |                                                         #
#      v             v                                                         #
# +----------+   +-------+                                                     #
# | Waveform |   | Event | <== The results are then forwarded                  #
# +----------+   +-------+     to the rest of the DAQ                          #
#                              They may be discarded by the                    #
#                              user functions, to eliminate                    #
#                              unwanted events.                                #
#                                                                              #
################################################################################
```

## Analysis functions
Two analysis functions are foreseen in the structure:

1. `timestamp_analysis()` that determines the timestamp information of the pulse;
2. `energy_analysis()` that determines the energy information of the pulse;

The two analyses were separated in order to simplify code reuse.
The user might want to perform both analyses in either one of the two functions, in order to optimize the computational time.
The functions may be different for each acquisition channel, allowing the coexistence of different detectors on the same digitizer.
A pulse may also be discarded by the user functions, in order to eliminate unwanted/noise events.

## Configuration
The configuration file is in the [JSON](http://www.json.org/) format.
For each channel the user shall provide the filename of the libraries.
The filenames shall follow the conventions of the UNIX dynamic linking functions; thus if the filename contains a slash (`/`) then the file is searched on the given path, if there is no slash then the library is searched in the standard system library folders.
If the user wants to use a local file in the current directory the filename shall start with `./` (_e.g._ `./libCFD.so`).
The libraries may be modified and reloaded at runtime.
To inform `waan` that the libraries are to be reloaded, the provided python script [`send_configuration.py`](./send_configuration.py) can send the updated configuration file to a running instance of `waan`.
An update of the configuration implies a reload of the user libraries.

Each channel configuration may contain a user defined configuration.
This information is then passed to the `init` functions of the user libraries, so the user does not have to hardcode the functions parameters.

## Libraries compilation
The user libraries shall be implemented with a C99 interface, so no C++.
The [`src/`](./src/) folder contains some documented examples.
The user may compile the custom library with the provided Makefile:
```
user@host$ make libuser.so
```
Where the source code of the library would be `libuser.c`.

## Example libraries
These example libraries are provided:

- `libNull.c`: Simple `timestamp_analysis()` function that forwards the waveform to the energy analysis without doing anything. It is useful if the user is not interested in determining timing information from the pulse or if the user wants to perform all the analysis in the energy analysis step.
- `libStpAvg.c`: Calculates the energy information of a exponentially decaying pulse, by compensating the decay and determining its height with simple averages.
- `libCRRC4.c`: Calculates the energy information of a exponentially decaying pulse, by compensating the decay and then applying a recursive CR-RC^4 filter.
- `libPSD.c`: Calculates the energy and Pulse Shape information of a short pulse, by applying the double integration method.
