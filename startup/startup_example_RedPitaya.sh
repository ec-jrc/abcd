#! /bin/bash
# 
# Example of startup script

# In order to run abrp we need to inform where the Red Pitaya libraries are
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/redpitaya/lib/

# Check if the ABCD_FOLDER variable is set in the environment, otherwise set it here
if [[ -z "${ABCD_FOLDER}" ]]; then
    # The variable is not set, thus set it to the folder in which ABCD is installed
    ABCD_FOLDER="$HOME""/abcd/"
fi

# Folder in which data should be saved
# Check if the DATA_FOLDER variable is set in the environment, otherwise set it here
if [[ -z "${DATA_FOLDER}" ]]; then
    # The variable is not set, thus set it to the folder in which ABCD is installed
    DATA_FOLDER="${ABCD_FOLDER}""/data/"
fi


CURRENT_FOLDER="$PWD"

TODAY="`date "+%Y%m%d"`"
echo 'Today is '"$TODAY"

# Unsetting $TMUX in order to be able to launch new sessions from tmux
unset TMUX

if [[ ! -x "${ABCD_FOLDER}/waan/waan" ]]
then
    printf '\e[1m\e[31mERROR:\e[0m waan is not executable.\n'
    printf '\e[1m\e[32mSuggestions:\e[0m Was ABCD correctly compiled?\n'
    printf '             Is the ${ABCD_FOLDER} variable set correctly in the environment or in the script?\n'
else
    #This is needed only on older versions of tmux, if the -c option does not work
    #echo "Changing folder to: ""${ABCD_FOLDER}"
    #cd "${ABCD_FOLDER}"
    
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
    tmux new-window -d -c "${ABCD_FOLDER}/wit/" -P -t ABCD -n wit 'node app.js ./config.json'
    
    echo "Creating loggers window"
    tmux new-window -d -c "${ABCD_FOLDER}" -P -t ABCD -n loggers "./bin/read_events.py -S 'tcp://127.0.0.1:16180' -o log/abcd_events_""$TODAY"".log"
    tmux split-window -d -c "${ABCD_FOLDER}" -P -t ABCD:2.0 -h "./bin/read_events.py -S 'tcp://127.0.0.1:16183' -o log/hivo_events_""$TODAY"".log"
    tmux split-window -d -c "${ABCD_FOLDER}" -P -t ABCD:2.0 -h "./bin/read_events.py -S 'tcp://127.0.0.1:16185' -o log/dasa_events_""$TODAY"".log"
    tmux split-window -d -c "${ABCD_FOLDER}" -P -t ABCD:2.0 -h "./bin/read_events.py -S 'tcp://127.0.0.1:16187' -o log/spec_events_""$TODAY"".log"
    
    tmux select-layout -t ABCD:2 even-vertical
    
    echo "Waiting for node.js to start"
    sleep 2
    
    echo "Creating abrp window"
    tmux new-window -d -c "${ABCD_FOLDER}" -P -t ABCD -n abrp "./abrp/abrp -f abrp/configs/config_CeBr.json -D 'tcp://*:16207' -T 50"
    
    echo "Creating WaAn window"
    tmux new-window -d -c "${ABCD_FOLDER}/waan/" -P -t ABCD -n waan './waan -v -T 100 -A tcp://127.0.0.1:16207 -D tcp://*:16181 -f ./configs/config_RedPitaya_CeBr.json'
    
    echo "Creating DaSa window, folder: ${DATA_FOLDER}"
    tmux new-window -d -c "${DATA_FOLDER}" -P -t ABCD -n dasa "${ABCD_FOLDER}/dasa/dasa -v"
    
    echo "Creating WaDi window"
    tmux new-window -d -c "${ABCD_FOLDER}" -P -t ABCD -n wadi './wadi/wadi -v'
    
    echo "Creating tofcalc windows"
    tmux new-window -d -c "${ABCD_FOLDER}" -P -t ABCD -n tofcalc "./tofcalc/tofcalc -f ./tofcalc/configs/config_RedPitaya.json -n 0.0078"
    
    echo "Creating spec windows"
    tmux new-window -d -c "${ABCD_FOLDER}" -P -t ABCD -n spec "./spec/spec"
    
    echo "System started!"
    echo "Connect to GUI on addresses: http://127.0.0.1:8080/"
    
    # In case it is needed to start a browser as well
    #echo "Opening browser on GUI page"
    #xdg-open 'http://localhost:8080/'
fi
