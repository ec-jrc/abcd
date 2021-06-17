# waph - Waveforms to Pulse Height events
This module converts waveform datastreams to events datastreams, using a trapezoidal filter in order to find the pulse height.
The pulse height is stored in a PSD event data structure with two slightly different pulse heights, calculated according to two different algorithms.

## Usage
To run the module:

    ./waph [options] <config_file>

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

## Configuration file
The configuration file uses the [JSON](https://www.json.org/) format, there is an example file called `config.json`.
An example follows (**warning**: the annotations are not JSON conformant, therefore should not be present in the file).

    {
        "module": "waph",
        // Channels is an array of objects defining the configuration of each channel
        "channels": [
        // Each channel is defined as an object
        {
            // The ID of the channel that is to be configured
            "id": 0,
            // A channel may be disabled and therefore the calculations are not performed on it
            "enabled": true,
            // The width of the trapezoid slopes, in samples.
            "trapezoid_risetime": 1000,
            // The width of the trapezoid flat top, in samples.
            "trapezoid_flattop": 1000,
            // A scaling parameter, the peak height is divided by 2^(trapezoid_rescaling)
            "trapezoid_rescaling": 7,
            // The expected position of the trapezoid peak, the sample at that index
            // is going to be considered the peak height.
            "peaking_time": 2700,
            // Width of the baseline in the trapezoid filtered signal. It can be zero.
            "baseline_window": 0,
            // The signal decay time in samples, for the pole zero correction that
            // converts exponentially decaying signals to a step function.
            "decay_time": 90000,
            // Defines the pulse polarity, possible values: "positive" or "negative"
            "pulse_polarity": "positive",
            // Optional name that is ignored
            "name": "HPGe",
            // Optional notes that are ignored
            "note": "This channel is very important"
        }
        ]
    }

## Resulting events description
The resulting events use the same structure as the PSD events, but the meaning of the words is different:

 - `uint64_t timestamp`: Timestamp of the event as given by the digitizer;
 - `uint16_t trapezoid_height`: the pulse height calculated as the absolute maximum of the trapezoid;
 - `uint16_t peak_height`: the pulse height calculated as the sample at index `peaking_time`;
 - `uint16_t baseline`: value of the baseline;
 - `uint8_t channel`: channel number;
 - `uint8_t PUR`: unused.
