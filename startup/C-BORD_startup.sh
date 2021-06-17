#! /bin/bash

# Folder in which ABCD is installed
FOLDER="$HOME""/abcd/"
# Folder in which data should be saved
DATA_FOLDER="/mnt/Data/"

TODAY="`date "+%Y%m%d"`"
echo 'Today is '"$TODAY"

# Unsetting $TMUX in order to be able to launch new sessions from tmux
unset TMUX

#This is needed only on older versions of tmux, if the -c option does not work
#echo "Changing folder to: ""$FOLDER"
#cd "$FOLDER"

# Checking if another CBORD17 session is running
if [ "`tmux ls 2> /dev/null | grep CBORD17 | wc -l`" -gt 0 ]
then
    echo "Killing previous CBORD17 sessions"
    tmux kill-session -t CBORD17
    sleep 2
fi

echo "### Starting a new CBORD17 session"
tmux new-session -d -s CBORD17



echo "### Creating V1730 #0097 readout"
echo "Creating EFG window"
tmux new-window -d -c "$FOLDER""/efg/" -P -t CBORD17 -n efg10 'node efg.js -f C-BORD_defaults_V1730_0097.json'

#echo "Creating loggers window"
#tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n loggers "./bin/read_events.py -S 'tcp://127.0.0.1:16180' -o log/abcd_events_""$TODAY"".log"
#tmux split-window -d -c "$FOLDER" -P -t CBORD17:2.0 -h "./bin/read_events.py -S 'tcp://127.0.0.1:16183' -o log/hivo_events_""$TODAY"".log"
#tmux split-window -d -c "$FOLDER" -P -t CBORD17:2.0 -h "./bin/read_events.py -S 'tcp://127.0.0.1:16185' -o log/lmno_events_""$TODAY"".log"
#tmux split-window -d -c "$FOLDER" -P -t CBORD17:2.0 -h "./bin/read_events.py -S 'tcp://127.0.0.1:16187' -o log/pqrs_events_""$TODAY"".log"
#tmux select-layout -t CBORD17:2 even-vertical

echo "Waiting for node.js to start"
sleep 2

echo "Creating ABCD window"
#tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n abcd00 './abcd/abcd -c 1 -l 1 -n 0 -B 8192 -T 10 -D "tcp://*:15001" -S "tcp://*:15002" -C "tcp://*:15003" -f abcd/configs/V1730_0097_DPP-PSD_C-BORD.json'
tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n abcd00 './abcd/abcd -c 1 -l 1 -n 0 -B 8192 -T 10 -D "tcp://*:15001" -S "tcp://*:15002" -C "tcp://*:15003" -f abcd/configs/V1730_0097_DPP-PSD_C-BORD.json'
#tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n abcd00 './abcd/abcd -c 1 -l 1 -n 0 -B 8192 -T 10 -D "tcp://*:15001" -S "tcp://*:15002" -C "tcp://*:15003" -f abcd/configs/V1730_0097_DPP-PSD_C-BORD-NO-COIN.json'

echo "Creating LMNO window, folder: ""$DATA_FOLDER"
tmux new-window -d -c "$DATA_FOLDER" -P -t CBORD17 -n lmno00 "$FOLDER"'/lmno/lmno -A "tcp://127.0.0.1:15001" -s "tcp://127.0.0.1:15002" -S "tcp://*:15004" -C "tcp://*:15005"'

echo "Creating PQRS window"
tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n pqrs00 './pqrs/pqrs -A "tcp://127.0.0.1:15001" -D "tcp://*:15006" -S "tcp://*:15007" -C "tcp://*:15008"'

echo "Creating WaFi window"
tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n wafi00 './wafi/wafi -v -T 100 -S "tcp://127.0.0.1:15001" -P "tcp://*:15009"'

# This part is for calculating coincidences, but since they are already computed by the digitizer we do not need that
#echo "Creating Coincidence Filters windows"
#echo "Creating EFG window"
#tmux new-window -d -c "$FOLDER""/efg/" -P -t CBORD17 -n efg06 'node efg.js -f C-BORD_defaults_V1730_0097_Coincidence.json'

#echo "Creating CoFi window for Alpha SUM"
#tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n cofi00 "$FOLDER"'/cofi/cofi -v -S "tcp://127.0.0.1:15001" -P "tcp://*:15010" 0'
#echo "Creating CoFi window for Gamma Detectors"
#tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n cofi01 "$FOLDER"'/cofi/cofi -v -S "tcp://127.0.0.1:15010" -P "tcp://*:15011" 1 2 3 5 6 7 9 10 11 12 13 15'

#echo "Creating LMNO window, folder: ""$DATA_FOLDER"
#tmux new-window -d -c "$DATA_FOLDER" -P -t CBORD17 -n lmno01 "$FOLDER"'/lmno/lmno -A "tcp://127.0.0.1:15011" -s "tcp://127.0.0.1:15002" -S "tcp://*:15012" -C "tcp://*:15014"'

#echo "Creating PQRS window"
#tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n pqrs01 './pqrs/pqrs -A "tcp://127.0.0.1:15011" -D "tcp://*:15015" -S "tcp://*:15016" -C "tcp://*:15017"'

#echo "Creating WaFi window"
#tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n wafi01 './wafi/wafi -v -T 100 -S "tcp://127.0.0.1:15011" -P "tcp://*:15018"'



echo "### Creating V1730 #0228 readout"
echo "Creating EFG window"
tmux new-window -d -c "$FOLDER""/efg/" -P -t CBORD17 -n efg10 'node efg.js -f C-BORD_defaults_V1730_0228.json'

echo "Waiting for node.js to start"
sleep 2

echo "Creating ABCD window"
#tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n abcd10 './abcd/abcd -c 1 -l 0 -n 0 -B 8192 -T 10 -D "tcp://*:15101" -S "tcp://*:15102" -C "tcp://*:15103" -f abcd/configs/V1730_0228_DPP-PSD_C-BORD.json'
tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n abcd10 './abcd/abcd -c 1 -l 1 -n 1 -B 8192 -T 10 -D "tcp://*:15101" -S "tcp://*:15102" -C "tcp://*:15103" -f abcd/configs/V1730_0228_DPP-PSD_C-BORD.json'
#tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n abcd10 './abcd/abcd -c 1 -l 1 -n 1 -B 8192 -T 10 -D "tcp://*:15101" -S "tcp://*:15102" -C "tcp://*:15103" -f abcd/configs/V1730_0228_DPP-PSD_C-BORD-NO-COIN.json'

echo "Creating LMNO window, folder: ""$DATA_FOLDER"
tmux new-window -d -c "$DATA_FOLDER" -P -t CBORD17 -n lmno10 "$FOLDER"'/lmno/lmno -A "tcp://127.0.0.1:15101" -s "tcp://127.0.0.1:15102" -S "tcp://*:15104" -C "tcp://*:15105"'

echo "Creating PQRS window"
tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n pqrs10 './pqrs/pqrs -A "tcp://127.0.0.1:15101" -D "tcp://*:15106" -S "tcp://*:15107" -C "tcp://*:15108"'

echo "Creating WaFi window"
tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n wafi10 './wafi/wafi -v -T 100 -S "tcp://127.0.0.1:15101" -P "tcp://*:15109"'

# This part is for calculating coincidences, but since they are already computed by the digitizer we do not need that
#echo "Creating Coincidence Filters windows"
#echo "Creating EFG window"
#tmux new-window -d -c "$FOLDER""/efg/" -P -t CBORD17 -n efg17 'node efg.js -f C-BORD_defaults_V1730_0228_Coincidence.json'

#echo "Creating CoFi window for Alpha SUM"
#tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n cofi10 "$FOLDER"'/cofi/cofi -v -S "tcp://127.0.0.1:15101" -P "tcp://*:15110" 0'
#echo "Creating CoFi window for Gamma Detectors"
#tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n cofi11 "$FOLDER"'/cofi/cofi -v -S "tcp://127.0.0.1:15110" -P "tcp://*:15111" 1 2 3 5 6 7 9 10 11 12 13 14'

#echo "Creating LMNO window, folder: ""$DATA_FOLDER"
#tmux new-window -d -c "$DATA_FOLDER" -P -t CBORD17 -n lmno11 "$FOLDER"'/lmno/lmno -A "tcp://127.0.0.1:15111" -s "tcp://127.0.0.1:15102" -S "tcp://*:15112" -C "tcp://*:15114"'

#echo "Creating PQRS window"
#tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n pqrs11 './pqrs/pqrs -A "tcp://127.0.0.1:15111" -D "tcp://*:15115" -S "tcp://*:15116" -C "tcp://*:15117"'

#echo "Creating WaFi window"
#tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n wafi11 './wafi/wafi -v -T 100 -S "tcp://127.0.0.1:15111" -P "tcp://*:15118"'



echo "### Creating High-Voltages windows"
echo "Creating EFG windows"
tmux new-window -d -c "$FOLDER""/efg/" -P -t CBORD17 -n efg20 'node efg.js -f C-BORD_defaults_V6533_0336.json'
tmux new-window -d -c "$FOLDER""/efg/" -P -t CBORD17 -n efg30 'node efg.js -f C-BORD_defaults_V6533_0369.json'
tmux new-window -d -c "$FOLDER""/efg/" -P -t CBORD17 -n efg40 'node efg.js -f C-BORD_defaults_V6533_0392.json'
tmux new-window -d -c "$FOLDER""/efg/" -P -t CBORD17 -n efg50 'node efg.js -f C-BORD_defaults_V6533_0398.json'

echo "Waiting for node.js to start"
sleep 2

echo "Creating HiVo windows"
tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n hivo20 './hivo/hivo -c 1 -l 0 -n 0 -V "0x3360000" -T 500 -S "tcp://*:15201" -C "tcp://*:15202" -f hivo/configs/V6533_0336_C-BORD.json'
tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n hivo30 './hivo/hivo -c 1 -l 0 -n 0 -V "0x3690000" -T 500 -S "tcp://*:15301" -C "tcp://*:15302" -f hivo/configs/V6533_0369_C-BORD.json'
tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n hivo40 './hivo/hivo -c 1 -l 0 -n 0 -V "0x3920000" -T 500 -S "tcp://*:15401" -C "tcp://*:15402" -f hivo/configs/V6533_0392_C-BORD.json'
tmux new-window -d -c "$FOLDER" -P -t CBORD17 -n hivo50 './hivo/hivo -c 1 -l 0 -n 0 -V "0x3980000" -T 500 -S "tcp://*:15501" -C "tcp://*:15502" -f hivo/configs/V6533_0398_C-BORD.json'

echo "### Creating the on-line ToF calculator windows"
TOFCALC_FOLDER="$FOLDER""/tofcalc/"

echo "Creating GUI windows"
tmux new-window -d -c "$TOFCALC_FOLDER" -P -t CBORD17 -n tofcalcgui00 'node tofcalc.js -f C-BORD_tofcalc_V1730_0097.json'
tmux new-window -d -c "$TOFCALC_FOLDER" -P -t CBORD17 -n tofcalcgui10 'node tofcalc.js -f C-BORD_tofcalc_V1730_0228.json'

echo "Waiting for node.js to start"
sleep 2

echo "Creating tofcalc windows"
tmux new-window -d -c "$TOFCALC_FOLDER" -P -t CBORD17 -n tofcalc00 "./tofcalc -A 'tcp://127.0.0.1:15001' -D 'tcp://*:15601' -S 'tcp://*:15602' -C 'tcp://*:15603' -f C-BORD_tofcalc_V1730_0097.json"
tmux new-window -d -c "$TOFCALC_FOLDER" -P -t CBORD17 -n tofcalc00 "./tofcalc -A 'tcp://127.0.0.1:15101' -D 'tcp://*:15701' -S 'tcp://*:15702' -C 'tcp://*:15703' -f C-BORD_tofcalc_V1730_0228.json"
