#! /bin/bash
# 
# Example of startup script

# Folder in which ABCD is installed
ABCD_FOLDER="$HOME""/abcd/"
# Folder in which data should be saved
DATA_FOLDER="$ABCD_FOLDER""/data/"

TODAY="`date "+%Y%m%d"`"
echo 'Today is '"$TODAY"

# Unsetting $TMUX in order to be able to launch new sessions from tmux
unset TMUX

#This is needed only on older versions of tmux, if the -c option does not work
#echo "Changing folder to: ""$ABCD_FOLDER"
#cd "$ABCD_FOLDER"

# Checking if another ABCD session is running
if [ "`tmux ls 2> /dev/null | grep ABCD | wc -l`" -gt 0 ]
then
    echo "Kiling previous ABCD sessions"
    tmux kill-session -t ABCD
    sleep 2
fi

echo "Starting a new ABCD session"
tmux new-session -d -s ABCD

echo "Creating GUI windows"
tmux new-window -d -c "$ABCD_FOLDER""/efg/" -P -t ABCD -n efg 'node efg.js'

tmux new-window -d -c "${ABCD_FOLDER}/web_interface/" -P -t ABCD -n guispec "node web_interface.js -v -m spec -f ${ABCD_FOLDER}/spec/config.json -d ${ABCD_FOLDER}/spec/ -p 8081"
tmux new-window -d -c "${ABCD_FOLDER}/web_interface/" -P -t ABCD -n guitof  "node web_interface.js -v -m tofcalc -f ${ABCD_FOLDER}/tofcalc/config.json -d ${ABCD_FOLDER}/tofcalc/ -p 8082"

echo "Creating loggers window"
tmux new-window -d -c "$ABCD_FOLDER" -P -t ABCD -n loggers "./bin/read_events.py -S 'tcp://127.0.0.1:16180' -o log/abcd_events_""$TODAY"".log"
tmux split-window -d -c "$ABCD_FOLDER" -P -t ABCD:4.0 -h "./bin/read_events.py -S 'tcp://127.0.0.1:16183' -o log/hivo_events_""$TODAY"".log"
tmux split-window -d -c "$ABCD_FOLDER" -P -t ABCD:4.0 -h "./bin/read_events.py -S 'tcp://127.0.0.1:16185' -o log/dasa_events_""$TODAY"".log"
tmux split-window -d -c "$ABCD_FOLDER" -P -t ABCD:4.0 -h "./bin/read_events.py -S 'tcp://127.0.0.1:16187' -o log/spec_events_""$TODAY"".log"

tmux select-layout -t ABCD:4 even-vertical

echo "Waiting for node.js to start"
sleep 2

echo "Creating ABCD window"
# Example for a desktop digitizer
#tmux new-window -d -c "$ABCD_FOLDER" -P -t ABCD -n abcd './abcd/abcd -c 0 -l 0 -n 0 -B 8192 -T 50 -f abcd/configs/DT5751_DPP-PSD_NaI.json'
# Example for a VME digitizer with USB bridge
#tmux new-window -d -c "$ABCD_FOLDER" -P -t ABCD -n abcd './abcd/abcd -c 0 -l 0 -n 0 -V "0x210000" -B 8192 -T 50 -f abcd/v1730_NaI_Coincidence_DCFD.json'
# Example for a VME digitizer with optical bridge
#tmux new-window -d -c "$ABCD_FOLDER" -P -t ABCD -n abcd './abcd/abcd -c 1 -l 0 -n 0 -V "0x210000" -B 8192 -T 50 -f abcd/v1730_NaI_Coincidence_DCFD.json'
# Example for a VME digitizer with direct optical link
#tmux new-window -d -c "$ABCD_FOLDER" -P -t ABCD -n abcd './abcd/abcd -c 1 -l 0 -n 0 -B 8192 -T 10 -f abcd/v1730_NaI_Coincidence_DCFD.json'

echo "Creating DaSa window, folder: ""$DATA_FOLDER"
tmux new-window -d -c "$DATA_FOLDER" -P -t ABCD -n dasa "$ABCD_FOLDER"'/dasa/dasa'

echo "Creating WaFi window"
tmux new-window -d -c "$ABCD_FOLDER" -P -t ABCD -n wafi './wafi/wafi -v -T 100'

echo "Creating tofcalc windows"
tmux new-window -d -c "$ABCD_FOLDER" -P -t ABCD -n tofcalc "./tofcalc/tofcalc -f ./tofcalc/config.json -n 0.00195312"

echo "Creating spec windows"
tmux new-window -d -c "$ABCD_FOLDER" -P -t ABCD -n spec "./spec/spec"

echo "System started!"
echo "Connect to GUI on addresses: http://127.0.0.1:8080/ http://127.0.0.1:8081/ http://127.0.0.1:8082/"

# In case it is needed to start a browser as well
#echo "Opening browser on EFG page"
#xdg-open 'http://localhost:8080/'
