#! /bin/bash
#
# Example of startup script to determine the effects of temporal coincidences between two detectors

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

# The file to be replayed is passed as an argument to this script
if [[ -z "$1" ]]
then
    # If no argument is given, then use the default example file
    FILE_NAME="${ABCD_FOLDER}/data/example_data_SPD214_Ch4_BGO_anticompton_Ch5_LaCl_background_events.ade"
else
    FILE_NAME="$1"
fi

# This check if this variable is set in the environment.
# It should contain the space-separated list of reference channels for the coincidence determination
if [[ -z "${REFERENCE_CHANNELS}" ]]
then
    REFERENCE_CHANNELS="4"
fi

CURRENT_FOLDER="$PWD"

TODAY="`date "+%Y%m%d"`"
echo 'Today is '"$TODAY"

# Unsetting $TMUX in order to be able to launch new sessions from tmux
unset TMUX

# Check if the cofi module is executable.
# This is an indication that ABCD was at least compiled and the ${ABCD_FOLDER} is correctly set.
if [[ ! -x "${ABCD_FOLDER}/cofi/cofi" ]]
then
    printf '\e[1m\e[31mERROR:\e[0m cofi is not executable.\n'
    printf '\e[1m\e[32mSuggestions:\e[0m Was ABCD correctly compiled?\n'
    printf '             Is the ${ABCD_FOLDER} variable set correctly in the environment or in the script?\n'
else
    #This is needed only on older versions of tmux, if the -c option does not work
    #echo "Changing folder to: ""${ABCD_FOLDER}"
    #cd "${ABCD_FOLDER}"

    echo "Replaying data file: ${FILE_NAME}"

    # Checking if another ABCD session is running
    if [ "`tmux ls 2> /dev/null | grep ABCD | wc -l`" -gt 0 ]
    then
        echo "Kiling previous ABCD sessions"
        tmux kill-session -t ABCD
        sleep 2
    fi

    echo "Starting a new ABCD session"
    tmux new-session -d -s ABCD

    echo "Creating the window for the GUI webserver"
    # The webserver uses a configuration different from the default one for this example.
    # This configuration informs it that there will be several parallel instances of some modules.
    tmux new-window -d -c "${ABCD_FOLDER}/wit/" -P -t ABCD -n wit 'node app.js ./config_coincidences.json'

    echo "Creating loggers window"
    tmux new-window -d -c "${ABCD_FOLDER}" -P -t ABCD -n loggers "./bin/read_events.py -S 'tcp://127.0.0.1:16180' -o log/abcd_events_""$TODAY"".log"
    tmux split-window -d -c "${ABCD_FOLDER}" -P -t ABCD:2.0 -h "./bin/read_events.py -S 'tcp://127.0.0.1:16185' -o log/dasa_events_""$TODAY"".log"
    tmux split-window -d -c "${ABCD_FOLDER}" -P -t ABCD:2.0 -h "./bin/read_events.py -S 'tcp://127.0.0.1:16206' -o log/waan_events_""$TODAY"".log"

    tmux select-layout -t ABCD:2 even-vertical

    echo "Waiting for node.js to start"
    sleep 2

    # Since we are replaying events files, activating waan would be problematic,
    # because it reads the data stream and will discard the processed events,
    # because it will recalculate them.
    # We will leave this here but commented as a placeholder
    #echo "Creating waan window"
    #tmux new-window -d -c "${ABCD_FOLDER}/waan/" -P -t ABCD -n waan './waan -v -T 100 -A tcp://127.0.0.1:16207 -D tcp://*:16181 -f ./configs/config_example_data.json'

    # This module is needed for the waveforms visualization, in this example is
    # useless but it uses very little resources and we can leave it here
    echo "Creating wadi window"
    tmux new-window -d -c "${ABCD_FOLDER}" -P -t ABCD -n wadi './wadi/wadi'

    # This module reads the singles data stream, but it calculates the time coincidences by itself.
    # It does not need to be attached after a cofi module.
    echo "Creating tofcalc window"
    tmux new-window -d -c "${ABCD_FOLDER}" -P -t ABCD -n tofcalc "./tofcalc/tofcalc -f ./tofcalc/configs/SPD214_BGO_LaCl.json"

    # This module generates the coincidences and anticoincidences datastreams
    echo "Creating cofi window"
    tmux new-window -d -c "${ABCD_FOLDER}" -P -t ABCD -n cofi "./cofi/cofi -A tcp://127.0.0.1:16181 -D tcp://*:17181 -N tcp://*:18181 -a -w 100 -n 0.0024414063 ${REFERENCE_CHANNELS}"
   
    # This is the first dasa that will see all the data being replayed.
    # It is connected to the default data port tcp://127.0.0.1:16181 and it is in parallel to cofi.
    echo "Creating dasa window 1, folder: ${DATA_FOLDER}"
    tmux new-window -d -c "${DATA_FOLDER}" -P -t ABCD -n dasa1 "${ABCD_FOLDER}/dasa/dasa"

    # This is the second dasa that will see only the coincidences data being replayed.
    # It is connected to the data port tcp://127.0.0.1:17181 of cofi.
    echo "Creating DaSa window 2, folder: ${DATA_FOLDER}"
    tmux new-window -d -c "${DATA_FOLDER}" -P -t ABCD -n dasa2 "${ABCD_FOLDER}/dasa/dasa -v -A tcp://127.0.0.1:17181 -S tcp://*:17185 -C tcp://*:17186"

    # This is the third dasa that will see only the anticoincidences data being replayed.
    # It is connected to the data port tcp://127.0.0.1:18181 of cofi.
    echo "Creating DaSa window 3, folder: ${DATA_FOLDER}"
    tmux new-window -d -c "${DATA_FOLDER}" -P -t ABCD -n dasa3 "${ABCD_FOLDER}/dasa/dasa -v -A tcp://127.0.0.1:18181 -S tcp://*:18185 -C tcp://*:18186"

    # This is the first spec that will see all the data being replayed.
    # It is connected to the default data port tcp://127.0.0.1:16181 and it is in parallel to cofi.
    echo "Creating spec window 1"
    tmux new-window -d -c "${ABCD_FOLDER}" -P -t ABCD -n spec1 "./spec/spec"

    # This is the second spec that will see only the coincidences data being replayed.
    # It is connected to the data port tcp://127.0.0.1:17181 of cofi.
    echo "Creating spec window 2"
    tmux new-window -d -c "${ABCD_FOLDER}" -P -t ABCD -n spec2 "./spec/spec -A tcp://127.0.0.1:17181 -S tcp://*:17187 -D tcp://*:17188 -C tcp://*:17189"

    # This is the third spec that will see only the anticoincidences data being replayed.
    # It is connected to the data port tcp://127.0.0.1:18181 of cofi.
    echo "Creating spec window 3"
    tmux new-window -d -c "${ABCD_FOLDER}" -P -t ABCD -n spec3 "./spec/spec -A tcp://127.0.0.1:18181 -S tcp://*:18187 -D tcp://*:18188 -C tcp://*:18189"

    echo "System started!"
    echo "Connect to GUI on addresses: http://127.0.0.1:8080/"

    # In case it is needed to start a browser as well
    #echo "Opening browser on GUI page"
    #xdg-open 'http://localhost:8080/'

    echo "Waiting for the framework to be ready for the replay..."
    sleep 5

    # We will replay only events from the events file, so we are using replay_events
    # When the replayer finishes the file it will quit, so we have to launch it as the last process.
    echo "Creating replayer window, file: ${FILE_NAME}"
    tmux new-window -d -c "${CURRENT_FOLDER}" -P -t ABCD -n replay "${ABCD_FOLDER}/replay/replay_events -v -D 'tcp://*:16181' -T 100 -B 2048 ${FILE_NAME}"
fi
