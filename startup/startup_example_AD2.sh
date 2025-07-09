#! /bin/bash
#
# Example of startup script

# Check if the ABCD_FOLDER variable is set in the environment, otherwise set it here
if [[ -z "${ABCD_FOLDER}" ]]; then
    # The variable is not set, thus the user should set it to the folder in which ABCD is installed
    ABCD_FOLDER="$HOME""/abcd/"
fi

# Folder in which data should be saved
# Check if the DATA_FOLDER variable is set in the environment, otherwise set it here
if [[ -z "${DATA_FOLDER}" ]]; then
    # The variable is not set, thus the user should set it to the data destination folder
    DATA_FOLDER="${ABCD_FOLDER}""/data/"
fi

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

    echo "Creating logger window"
    tmux new-window -d -c "${ABCD_FOLDER}" -P -t ABCD -n logger "python3 ./bin/log_status_events.py -v -o ${ABCD_FOLDER}/log/ABCD_status_events_${TODAY}.tsv -S 'tcp://127.0.0.1:16180' 'tcp://127.0.0.1:16185' 'tcp://127.0.0.1:16206'"

    echo "Waiting for node.js to start"
    sleep 2

    echo "Creating abad window"
    tmux new-window -d -c "${ABCD_FOLDER}" -P -t ABCD -n abad './abad2/abad2 -T 1 -D "tcp://*:16207" -f abad2/configs/AD2_SiPM.json'

    echo "Creating WaAn window"
    tmux new-window -d -c "${ABCD_FOLDER}/waan/" -P -t ABCD -n waan './waan -v -T 100 -A tcp://127.0.0.1:16207 -D tcp://*:16181 -f ./configs/config_RedPitaya_CeBr.json'

    echo "Creating DaSa window, folder: ${DATA_FOLDER}"
    tmux new-window -d -c "${DATA_FOLDER}" -P -t ABCD -n dasa "${ABCD_FOLDER}/dasa/dasa -v"

    echo "Creating WaDi windows"
    tmux new-window -d -c "${ABCD_FOLDER}" -P -t ABCD -n wadidigi './wadi/wadi -v -A tcp://127.0.0.1:16207'
    tmux new-window -d -c "${ABCD_FOLDER}" -P -t ABCD -n wadiwaan './wadi/wadi -v -A tcp://127.0.0.1:16181 -D tcp://*:17190'

    echo "Creating tofcalc window"
    tmux new-window -d -c "${ABCD_FOLDER}" -P -t ABCD -n tofcalc "./tofcalc/tofcalc -f ./tofcalc/configs/AD2_SiPM_LYSO.json"

    echo "Creating spec window"
    tmux new-window -d -c "${ABCD_FOLDER}" -P -t ABCD -n spec "./spec/spec"

    echo "System started!"
    echo "Connect to GUI on addresses: http://127.0.0.1:8080/"

    # In case it is needed to start a browser as well
    #echo "Opening browser on GUI page"
    #xdg-open 'http://localhost:8080/'
fi
