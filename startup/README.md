# Startup scripts examples

## Introduction
This folder contains some example scripts for starting up an ABCD instance.
New users are suggested to start with [`startup_example_replay.sh`](./startup_example_replay.sh), so they can see a working system with the example data.
**Warning**: All the scripts require to know where ABCD is installed. Modify the variable `$ABCD_FOLDER` accordingly in the scripts.

## Scripts description
 - [`startup_example_replay.sh`](./startup_example_replay.sh) shows a standard startup for a replay of previously acquired data. The script uses the example data in the [`data/`](../data/) folder. `waan` is used to process the waveforms. Optionally, the script may accept a data file as argument:
 ```
 user@daq:~/abcd/startup$ ./startup_example_replay.sh ~/abcd/data/example_data_DT5730_Ch2_LaBr3_Ch4_LYSO_Ch6_YAP_raw.adr.bz2
 ```
 - [`startup_example.sh`](./startup_example.sh) shows a standard startup for a digitizer that processes waveforms on board.
 - [`startup_example_AD2.sh`](./startup_example_AD2.sh) shows a standard startup for a digitizer that can only acquire waveforms (_i.e._ no processing is done by the digitizer). `waps` then processes the waveforms on the computer, obtaining the energy information. Finally, `dasa` can save the processed datastream.
 - [`startup_example_waan.sh`](./startup_example_waan.sh) shows a standard startup for a digitizer that can only acquire waveforms. `waan` then processes the waveforms on the computer, obtaining the energy information. On the other hand, `dasa` is attached to the original datastream and will not save processed data.
 - [`C-BORD_startup.sh`](./C-BORD_startup.sh) shows a rather complex startup with two digitizers being acquired in parallel.
 - [`stop_ABCD.sh`](./stop_ABCD.sh) is a script to stop a running instance of ABCD.
