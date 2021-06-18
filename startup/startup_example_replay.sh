#! /bin/bash
# 
# Example of startup script

# Folder in which ABCD is installed
FOLDER="$HOME""/abcd/"
# Folder in which data should be saved
DATA_FOLDER="$FOLDER""/data/"

FILE_NAME="$1"

if [[ "x${FILE_NAME}x" == "xx" ]]
then
    FILE_NAME="${FOLDER}/data/example_data_DT5730_Ch1_LaBr3_Ch6_CeBr3_Ch7_CeBr3_coincidence_raw.adr.bz2"
fi

echo "Replaying data file: ${FILE_NAME}"

TODAY="`date "+%Y%m%d"`"
echo 'Today is '"$TODAY"

# Unsetting $TMUX in order to be able to launch new sessions from tmux
unset TMUX

#This is needed only on older versions of tmux, if the -c option does not work
#echo "Changing folder to: ""$FOLDER"
#cd "$FOLDER"

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
tmux new-window -d -c "$FOLDER""/efg/" -P -t ABCD -n efg 'node efg.js'

tmux new-window -d -c "${FOLDER}/web_interface/" -P -t ABCD -n guispec "node web_interface.js -v -m spec -f ${FOLDER}/spec/config.json -d ${FOLDER}/spec/ -p 8081"
tmux new-window -d -c "${FOLDER}/web_interface/" -P -t ABCD -n guitof  "node web_interface.js -v -m tofcalc -f ${FOLDER}/tofcalc/config.json -d ${FOLDER}/tofcalc/ -p 8082"

echo "Creating loggers window"
tmux new-window -d -c "$FOLDER" -P -t ABCD -n loggers "./bin/read_events.py -S 'tcp://127.0.0.1:16180' -o log/abcd_events_""$TODAY"".log"
tmux split-window -d -c "$FOLDER" -P -t ABCD:4.0 -h "./bin/read_events.py -S 'tcp://127.0.0.1:16183' -o log/hivo_events_""$TODAY"".log"
tmux split-window -d -c "$FOLDER" -P -t ABCD:4.0 -h "./bin/read_events.py -S 'tcp://127.0.0.1:16185' -o log/dasa_events_""$TODAY"".log"
tmux split-window -d -c "$FOLDER" -P -t ABCD:4.0 -h "./bin/read_events.py -S 'tcp://127.0.0.1:16187' -o log/spec_events_""$TODAY"".log"

tmux select-layout -t ABCD:4 even-vertical

echo "Waiting for node.js to start"
sleep 2

echo "Creating replayer window"
tmux new-window -d -c "$FOLDER" -P -t ABCD -n abcd "python3 ./replay/replay_raw.py -c -T 200 ${FILE_NAME}"

echo "Creating DaSa window, folder: ""$DATA_FOLDER"
tmux new-window -d -c "$DATA_FOLDER" -P -t ABCD -n dasa "$FOLDER"'/dasa/dasa'

echo "Creating WaFi window"
tmux new-window -d -c "$FOLDER" -P -t ABCD -n wafi './wafi/wafi -v -T 100'

echo "Creating tofcalc windows"
tmux new-window -d -c "$FOLDER" -P -t ABCD -n tofcalc "./tofcalc/tofcalc -f ./tofcalc/config.json -n 0.00195312"

echo "Creating spec windows"
tmux new-window -d -c "$FOLDER" -P -t ABCD -n spec "./spec/spec"

echo "System started!"
echo "Connect to GUI on addresses: http://127.0.0.1:8080/ http://127.0.0.1:8081/ http://127.0.0.1:8082/"

# In case it is needed to start a browser as well
#echo "Opening browser on EFG page"
#xdg-open 'http://localhost:8080/'
