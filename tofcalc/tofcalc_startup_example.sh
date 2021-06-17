#! /bin/bash
# 
# Example of startup script

# Folder in which ABCD is installed
FOLDER="$HOME""/abcd/"

# The module name that you are starting up
MODULE='tofcalc'

TODAY="`date "+%Y%m%d"`"
echo 'Today is '"$TODAY"

# Unsetting $TMUX in order to be able to launch new sessions from tmux
unset TMUX

#This is needed only on older versions of tmux, if the -c option does not work
#echo "Changing folder to: ""$FOLDER"
#cd "$FOLDER"

# Checking if another tofcalc session is running
if [ "`tmux ls 2> /dev/null | grep $MODULE | wc -l`" -gt 0 ]
then
    echo "Killing previous ""$MODULE"" sessions"
    tmux kill-session -t $MODULE
    sleep 2
fi

echo "Starting a new ""$MODULE"" session"
tmux new-session -d -s $MODULE

echo "Creating GUI window"
tmux new-window -d -c "$FOLDER""/web_interface/" -P -t $MODULE -n gui 'node web_interface.js -v -m '"$MODULE"' -f '"$FOLDER"'/'"$MODULE"'/config_JRC.json -d '"$FOLDER"'/'"$MODULE"'/ -p 8082'

echo "Waiting for node.js to start"
sleep 2

echo "Creating ""$MODULE"" window"
tmux new-window -d -c "$FOLDER""/""$MODULE""/" -P -t $MODULE -n $MODULE "./""$MODULE"" -f ./config_JRC.json -n 0.00195312 -A 'tcp://127.0.0.1:16181'"

echo "System started!"
echo "Connect to GUI on address: http://127.0.0.1:8082"
