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

echo "Creating the window for the GUI webserver: WebInterfaceTwo"
tmux new-window -d -c "$FOLDER""/wit/" -P -t ABCD -n wit 'node app.js ./config.json'

echo "Creating loggers window"
tmux new-window -d -c "$FOLDER" -P -t ABCD -n loggers "./bin/read_events.py -S 'tcp://127.0.0.1:16180' -o log/abcd_events_""$TODAY"".log"
tmux split-window -d -c "$FOLDER" -P -t ABCD:2.0 -h "./bin/read_events.py -S 'tcp://127.0.0.1:16183' -o log/hivo_events_""$TODAY"".log"
tmux split-window -d -c "$FOLDER" -P -t ABCD:2.0 -h "./bin/read_events.py -S 'tcp://127.0.0.1:16185' -o log/dasa_events_""$TODAY"".log"
tmux split-window -d -c "$FOLDER" -P -t ABCD:2.0 -h "./bin/read_events.py -S 'tcp://127.0.0.1:16187' -o log/spec_events_""$TODAY"".log"

tmux select-layout -t ABCD:2 even-vertical

echo "Waiting for node.js to start"
sleep 2

echo "Creating replayer window"
tmux new-window -d -c "$FOLDER" -P -t ABCD -n abcd "python3 ./replay/replay_raw.py -c -D 'tcp://*:16207' -T 50 ${FILE_NAME}"

echo "Creating WaAn window"
tmux new-window -d -c "${FOLDER}/waan/" -P -t ABCD -n waan './waan -v -T 200 -A tcp://127.0.0.1:16207 -D tcp://*:16181 -f ./configs/config_example_data.json'

echo "Creating DaSa window, folder: ""$DATA_FOLDER"
tmux new-window -d -c "$DATA_FOLDER" -P -t ABCD -n dasa "$FOLDER"'/dasa/dasa'

echo "Creating WaFi window"
tmux new-window -d -c "$FOLDER" -P -t ABCD -n wafi './wadi/wadi -v'

echo "Creating tofcalc windows"
tmux new-window -d -c "$FOLDER" -P -t ABCD -n tofcalc "./tofcalc/tofcalc -f ./tofcalc/configs/config_example_data.json -n 0.00195312"

echo "Creating spec windows"
tmux new-window -d -c "$FOLDER" -P -t ABCD -n spec "./spec/spec"

echo "System started!"
echo "Connect to GUI on addresses: http://127.0.0.1:8080/"

# In case it is needed to start a browser as well
echo "Opening browser on GUI page"
xdg-open 'http://localhost:8080/'
