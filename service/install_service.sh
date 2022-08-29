#! /bin/bash

# The startup script to be used
STARTUP_SCRIPT="../startup/startup_example_replay.sh"
# The user that will run the instance at boot, not necessarily the current user
USERNAME="`whoami`"

while getopts "u:S:h" opt
do
    case $opt in
    h)
        echo "Usage: ""$0"" <options>"
        echo "    -h: Show this message"
        echo "    -u <username>: Using the specified user"
        echo "    -S <startup_script>: Using the specified startup script"
        exit 0
        ;;
    u)
        echo "Setting username to: ""$OPTARG"
        USERNAME="$OPTARG"
        ;;
    S)
        echo "Setting startup_script to: ""$OPTARG"
        STARTUP_SCRIPT="$OPTARG"
        ;;
    \?)
        echo "ERROR: Invalid option: -""$OPTARG"
        exit 1
        ;;
    :)
        echo "ERROR: Option -""$OPTARG"" requires an argument"
        exit 1
        ;;
    esac
done

STARTUP_SCRIPT=$(readlink -f ${STARTUP_SCRIPT})

echo "Startup script with full path: ""${STARTUP_SCRIPT}"
echo "Username: ""${USERNAME}"

sed -e "s;{{ username }};${USERNAME};" -e "s;{{ startup_script }};${STARTUP_SCRIPT};" abcd.service.in > abcd.service

sudo cp abcd.service /etc/systemd/system/. && sudo systemctl enable abcd && sudo systemctl start abcd
