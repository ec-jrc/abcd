#! /bin/bash

# Folder in which ABCD is installed
ABCD_FOLDER="$HOME""/abcd/"
STARTUP_SCRIPT="this_startup.sh"
USERNAME="`whoami`"

while getopts "u:F:S:h" opt
do
    case $opt in
    h)
        echo "Usage: ""$0"" <options>"
        echo "    -h: Show this message"
        echo "    -u <username>: Using the specified user"
        echo "    -F <ABCD_folder>: Using the specified ABCD folder"
        echo "    -S <startup_script>: Using the specified startup script in the ABCD_folder"
        exit 0
        ;;
    u)
        echo "Setting username to: ""$OPTARG"
        USERNAME="$OPTARG"
        ;;
    F)
        echo "Setting folder to: ""$OPTARG"
        ABCD_FOLDER="$OPTARG"
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

echo "ABCD folder: ""$ABCD_FOLDER"
echo "Startup script: ""$STARTUP_SCRIPT"
echo "Username: ""$USERNAME"

sed -e 's;{{ username }};'"$USERNAME"';' -e 's;{{ startup_script }};'"$STARTUP_SCRIPT"';' -e 's;{{ ABCD_folder }};'"$ABCD_FOLDER"';' abcd.service.in > abcd.service

sudo cp abcd.service /etc/systemd/system/. && sudo systemctl enable abcd && sudo systemctl start abcd
