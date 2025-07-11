import numpy as np
import struct

# Define the waveform header size in bytes
WAVEFORM_HEADER_SIZE = 14

# Define the format string for the header fields
HEADER_FORMAT = "<QBLB"

# Define a class to represent a waveform object
class Waveform:
    def __init__(self, timestamp, channel, samples_number, additional_waveforms, buffer):
        self.timestamp = timestamp
        self.channel = channel
        self.samples_number = samples_number
        self.additional_waveforms = additional_waveforms
        self.buffer = buffer

    def __str__(self):
        return f"Waveform(timestamp={self.timestamp}, channel={self.channel}, samples_number={self.samples_number}, additional_waveforms={self.additional_waveforms})"

    def get_samples(self):
        # Return a numpy array of the samples as uint16
        return np.frombuffer(self.buffer[WAVEFORM_HEADER_SIZE:WAVEFORM_HEADER_SIZE + 2 * self.samples_number], dtype=np.uint16)

    def get_additional_waveforms(self):
        # Return a list of numpy arrays of the additional waveforms as uint8
        result = []
        offset = WAVEFORM_HEADER_SIZE + 2 * self.samples_number
        for i in range(self.additional_waveforms):
            result.append(np.frombuffer(self.buffer[offset:offset + self.samples_number], dtype=np.uint8))
            offset += self.samples_number
        return result


# Define a function to load waveforms from a binary file
def load_waveforms(filename):
    # Open the file in binary mode
    with open(filename, "rb") as f:
        # Initialize an empty list to store the waveforms
        waveforms = []
        # Loop until the end of the file is reached
        while True:
            # Read the header bytes
            header_bytes = f.read(WAVEFORM_HEADER_SIZE)
            # If the header bytes are empty, break the loop
            if not header_bytes:
                break
            # Unpack the header fields using the format string
            timestamp, channel, samples_number, additional_waveforms = struct.unpack(HEADER_FORMAT, header_bytes)
            # Calculate the size of the buffer in bytes
            buffer_size = WAVEFORM_HEADER_SIZE + 2 * samples_number + additional_waveforms * samples_number
            # Read the buffer bytes
            buffer_bytes = f.read(buffer_size - WAVEFORM_HEADER_SIZE)
            # If the buffer bytes are empty or less than expected, raise an exception
            if not buffer_bytes or len(buffer_bytes) < buffer_size - WAVEFORM_HEADER_SIZE:
                raise Exception("Invalid or incomplete waveform data")
            # Concatenate the header and buffer bytes
            buffer = header_bytes + buffer_bytes
            # Create a waveform object from the fields and buffer
            waveform = Waveform(timestamp, channel, samples_number, additional_waveforms, buffer)
            # Append the waveform object to the list
            waveforms.append(waveform)
        # Return the list of waveforms
        return waveforms


# Test the function with an example file name (change this to your actual file name)
waveforms = load_waveforms("waveforms.bin")

# Print some information about the loaded waveforms
print(f"Number of waveforms: {len(waveforms)}")
for i, waveform in enumerate(waveforms):
    print(f"Waveform {i}: {waveform}")
    print(f"Samples: {waveform.get_samples()}")
    print(f"Additional waveforms: {waveform.get_additional_waveforms()}")
