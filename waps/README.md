# waps - Waveforms to PSD events
This module converts waveform datastreams to events datastreams.
It calculates the PSD parameters for each event (_i.e_ short and long charge integrals).

## Usage
To run the module:

    ./waps [options] <config_file>

A config file is required for the execution.

### Usage options
* `-h`: Shows the help message.
* `-v`: Set verbose execution.
* `-V`: Set verbose execution with more output
* `-S <address>`: Set the SUB` socket address, _i.e._ the address from which waps will read the waveforms datastream.
* `-P <address>`: Set the PUB socket address, _i.e._ the address to which waps will publish the calculated events.
* `-T <period>`: Set base period in milliseconds.
* `-w`: Enable waveforms forwarding, so that also the incoming waveforms are published through the PUB socket.
* `-g`: Enable gates in the forwarded waveforms. The integration gates are calculated and added to the forwarded waveforms.
* `-l <selection_library>`: Load a user supplied library to select events.

## Configuration file
The configuration file uses the [JSON](https://www.json.org/) format, there is an example file called `config.json`.
An example follows (**warning**: the annotations are not JSON conformant, therefore should not be present in the file).

    {
        "module": "waps",
        // Channels is an array of objects defining the configuration of each channel
        "channels": [
        // Each channel is defined as an object
        {
            // The ID of the channel that is to be configured
            "id": 0,
            // A channel may be disabled and therefore the calculations are not performed on it
            "enabled": true,
            // The charge_sensitivity is used to define the dynamic range, it divides by powers of 4 the calculated integral.
            "charge_sensitivity": 1,
            // Corresponds to the delay in clock samples between the waveform start and the threshold crossing sample
            "pretrigger": 160,
            // Sets the number of clock samples before the threshold crossing to start the integration gates.
            "pregate": 30,
            // The length of the short gate in samples.
            "short_gate": 35,
            // The length of the long gate in samples.
            "long_gate": 200,
            // Defines the pulse polarity, possible values: "positive" or "negative"
            "pulse_polarity": "negative",
            // Optional name that is ignored
            "name": "Plastic scintillator",
            // The digital constant fraction configuration is in an object by itself
            "CFD": {
                // The CFD may be disabled and the original timestamp is kept
                "enabled": true,
                // Number of clock samples for the smoothing
                "smooth_samples": 10,
                // The fraction of the CFD
                "fraction": 0.5,
                // The delay of the CFD
                "delay": 10,
                // Number of samples used for the linear interpolation to determine the zero-crossing position.
                // It is always rounded to the next odd number.
                "zero_crossing_samples": 4,
            },
            // Optional notes that are ignored
            "note": "This channel is very important"
        }
        ]
    }

## Events selection library
Users may define a custom events selection function, that is used to select the events according to the user needs.
The function shall be compiled as a C shared library that can be loaded at run-time by the `waps` executable.
The library API is defined in the [`selection.h`](./selection.h) header file:

    #ifndef __SELECTION_H__
    #define __SELECTION_H__

    #include <stdint.h>
    #include <stdbool.h>

    void *select_init();

    bool select_event(uint32_t samples_number,
                      const uint16_t *samples,
                      size_t baseline_end,
                      uint64_t timestamp,
                      double qshort,
                      double qlong,
                      double baseline,
                      uint8_t channel,
                      uint8_t PUR,
                      struct *event_PSD event,
                      void *user_data);

    int select_close(void *user_data);

    #endif
    
The file [`libexample.c`](./libexample.c) shows an example of a user-defined library that selects events according to a energy and PSD threshold.

### Library development how-to
#### API implementation
The user shall define a `select_init()` function that implements the initialization of the events selection function.
It shall return the pointer to a memory block containing all the user-defined settings that the `select_event()` function needs.
For instance it could read the settings from a file and return a pointer to a `struct` containing the settings.
`waps` does not check the validity of the returned pointer, as the user could choose not to return any pointer if it is not needed.

The `select_event()` function receives from `waps` all the relevant parameters associated to the event and a pointer to the computed event.
The user has the option to modify the event itself in the function.
It shall return a `bool` value that is `true` if the event is to be kept, `false` otherwise.
If the `user_data` is needed, the user should take care to check its validity.

The `select_close()` function is called at the end of the execution of `waps` in order to perform the clean-up of the memory allocated by `select_init()`.


#### Compilation
The user may compile the library with the supplied makefile.
For instance, if the user prepared the source code in the `libuser.c` file, the compilation can be as follows:

    make libuser.so
    
Mind the different file extension: the source code shall be a `.c` file and the library a `.so` file.

#### Library loading
In order to load the library the flag `-l` shall be used:

    ./waps -l ./libuser.so
    
**Warning:** The library is located according to the standard rules of the OS.
Therefore if the filename does not contain any `/` (slash) the library is searched in the standard libraries folders (_i.e._ in the folders defined in the `LD_LIBRARY_PATH` environment varible), otherwise it is searched as a local file.
