# Startup scripts

This folder contains some example scripts for starting up an ABCD instance.

 - [`startup_example.sh`](./startup_example.sh) shows a standard startup for a digitizer that processes waveforms on board.
 - [`startup_example_AD2.sh`](./startup_example_AD2.sh) shows a standard startup for a digitizer that only acquires waveforms, that need to be processed in the computer.
 - [`startup_example_replay.sh`](./startup_example_replay.sh) shows a standard startup for a replay of previously acquired data. To ease the replay, the scripts accepts a data file as argument:
 ```
 user@daq:~/abcd/startup$ ./startup_example_replay.sh ~/abcd/data/example_data_DT5730_Ch2_LaBr3_Ch4_LYSO_Ch6_YAP_raw.adr.bz2
 ```
 - [`C-BORD_startup.sh`](./C-BORD_startup.sh) shows a rather complex startup with two digitizers being acquired in parallel.
 - [`stop_ABCD.sh`](./stop_ABCD.sh) is a script to stop a running instance of ABCD.
