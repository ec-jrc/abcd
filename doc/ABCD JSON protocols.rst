.. role:: json(code)
    :language: json

ABCD JSON protocols (TODO)
==========================

Commands
--------

Command sockets are always `PULL` sockets, so the sockets sending commands to them should be `PUSH` sockets.
The general format for commands is:

.. code-block:: json

    {
        "command": command,
        "arguments": kwargs,
        "msg_ID": msg_counter,
        "timestamp": now
    }

Where:

- :json:`command` is a string representing the command;
- :json:`kwargs` is a JSON formatted object with the specific arguments of the command;
- :json:`msg_ID` is a counter of the sent messages (normally ignored but could be useful for debugging pursposes);
- :json:`timestamp` is the timestamp of the sent messages in the ISO 8601 format (normally ignored but could be useful for debugging pursposes);

Digitizer interfaces
--------------------

Commands are:

1. :json:`"start"`: To start a data acquisition.
2. :json:`"stop"`: To stop the data acquisiton.
3. :json:`"reconfigure"`: To reconfigure a digitizer. It requires the :json:`arguments` entry, that depends on the specific digitizer used.

Data saving
-----------

Commands are:

1. :json:`"start"`: To start the data saving to a file. It requires the :json:`arguments` entry as specified below.
2. :json:`"stop"`: To stop the data saving.

The :json:`"start"` command requires the file name and which kind of files need to be saved.
The format is defined as:

.. code-block:: json

    {
        "command": "start",
        "arguments": {
            "file_name": filename
            "enable": {
                "events": (true|false),
                "waveforms": (true|false),
                "raw": (true|false),
            }
        },
        "msg_ID": msg_counter,
        "timestamp": now
    }

The :json:`filename` should be a string of the desired file name, without extension.
The :json:`"enable"` entry contains three boolean entries that define which file types should be saved.
It is always advisable to start the data saving before the start of the data acquisition, otherwise data would be lost.
Moreover, the raw files contain the additional information of the start and stop timestamps, which are useful for the experiment logbooks.
