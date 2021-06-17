#! /bin/bash

#  (C) Copyright 2019 Cristiano Lino Fontana
#
#  This file is part of ABCD.
#
#  ABCD is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  ABCD is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with ABCD.  If not, see <http://www.gnu.org/licenses/>.


function print_message {
    message=$1
    length=${#message}
    let 'length += 3'
    # Setting bold red
    printf '\e[1m\e[31m'
    printf '#%.0s' $(seq -s' ' 0 $length)
    printf '\n'
    printf '# '
    # Resetting bold and color
    #printf '\e[21m\e[39m'
    # Resetting all attributes
    printf '\e[0m'
    printf '%s' "${message}"
    # Setting bold red
    printf '\e[1m\e[31m'
    printf ' #\n' "${message}"
    printf '#%.0s' $(seq -s' ' 0 $length)
    printf '\n'
    # Resetting all attributes
    printf '\e[0m'
}

kernel_name="$(uname -s)"

print_message "Kernel name: ${kernel_name}"

if [[ "${kernel_name}" == "Linux" ]]
then
    print_message "Linux system found!"

    if [[ -n "$(command -v lsb_release)" ]]
    then
        distribution="$(lsb_release -s -i)"
        release="$(lsb_release -s -r)"

        print_message "Detected distribution: ${distribution}; release: ${release}"

        if [[ "$distribution" == "Fedora" ]]
        then
            print_message "Fedora system found!"

            print_message "Upgrading system..."
            dnf upgrade

            if [[ 24 -le "${release}" ]]
            then
                print_message "Installing required packages, for Fedora..."
                dnf install redhat-lsb vim-enhanced git tmux clang zeromq zeromq-devel jsoncpp jsoncpp-devel jansson-devel jansson zlib zlib-devel bzip2-libs bzip2-devel python3 python3-zmq nodejs node-gyp npm kernel-devel dkms elfutils-libelf-devel gsl gsl-devel
            else
                print_message "Unexpected ""$distribution"" version, you will have to install packages manually"
            fi
        elif [ "$distribution" == "CentOS" ]
        then
            print_message "CentOS system found!"

            print_message "Upgrading system..."
            dnf upgrade

            if [[ 8 -le "${release}" ]]
            then
                # The EPEL repository is for jsoncpp
                print_message "Enabling EPEL repository"

                ## The CentOS wiki suggests this, but for earlier releases:
                #dnf --enablerepo=extras install epel-release
                # The Fedora wiki suggests this:
                dnf install https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm
                # This is needed for the EPEL repository, according to the Fedora wiki,
                # but it looks more like something for RHEL.
                #subscription-manager repos --enable "codeready-builder-for-rhel-8-*-rpms"
                dnf config-manager --set-enabled PowerTools

                print_message "Upgrading again the system..."
                dnf upgrade

                print_message "Installing required packages, for CentOS..."
                dnf install redhat-lsb vim-enhanced git tmux clang jansson-devel jansson zlib zlib-devel bzip2-libs bzip2-devel nodejs npm kernel-devel kernel-headers elfutils-libelf-devel gsl gsl-devel jsoncpp jsoncpp-devel cmake zeromq zeromq-devel python38 python3-zmq wget dkms
            else
                print_message "Unexpected ""$distribution"" version, you will have to install packages manually"
            fi
        elif [ "$distribution" == "Ubuntu" ]
        then
            print_message "Ubuntu system found!"

            release="${release%.*}"

            print_message "Major release: ${release}"

            print_message "Upgrading system..."
            apt-get update
            apt-get upgrade

            if expr 14 '<=' "$release" '&' "$release" "<" 16 &>/dev/null
            then
                print_message "Installing required packages, for Ubuntu 14 trusty..."
                apt-get install lsb-release vim git tmux clang libzmq3 libzmq3-dev libjsoncpp-dev libjsoncpp0 libjansson-dev libjansson4 zlib1g zlib1g-dev libbz2-1.0 libbz2-dev python3 python3-zmq linux-headers-generic build-essential dkms libgsl0-dev

                print_message "To use Node.js and NPM we need to install them separately"

                print_message "Downloading a setup script for Node.js 10.x"
                wget https://deb.nodesource.com/setup_10.x -O nodejs_setup10.x.sh

                print_message "Running the script"
                source nodejs_setup10.x.sh

                apt-get install nodejs
            elif expr 16 '<=' "$release" '&' "$release" "<" 17 &>/dev/null
            then
                print_message "Installing required packages, for Ubuntu 16 xenial..."
                apt-get install lsb-release vim git tmux clang libzmq5 libzmq3-dev libjsoncpp-dev libjsoncpp1 libjansson-dev libjansson4 zlib1g zlib1g-dev libbz2-1.0 libbz2-dev python3 python3-zmq linux-headers-generic build-essential dkms libgsl-dev

                print_message "To use Node.js and NPM we need to install them separately"

                print_message "Downloading a setup script for Node.js 10.x"
                wget https://deb.nodesource.com/setup_10.x -O nodejs_setup10.x.sh

                print_message "Running the script"
                source nodejs_setup10.x.sh

                apt-get install nodejs
            elif expr 17 '<=' "$release" &>/dev/null
            then
                print_message "Installing required packages, for Ubuntu 17 artful and on..."
                apt-get install lsb-release vim git tmux clang libzmq5 libzmq3-dev libjsoncpp-dev libjsoncpp1 libjansson-dev libjansson4 zlib1g zlib1g-dev libbz2-1.0 libbz2-dev python3 python3-zmq nodejs npm linux-headers-generic build-essential dkms libgsl-dev
            else
                print_message "Unexpected ""$distribution"" version, you will have to install packages manually"
            fi
        elif [ "$distribution" == "Debian" ]
        then
            print_message "Debian system found!"

            print_message "Upgrading system..."
            apt-get update
            apt-get upgrade

            if expr 9 '<=' "$release" '&' "$release" "<" 10 &>/dev/null
            then
                print_message "Installing required packages, for Debian 9 Stretch..."
                apt-get install lsb-release vim git tmux clang libzmq5 libzmq3-dev libjsoncpp-dev libjsoncpp1 libjansson-dev libjansson4 zlib1g zlib1g-dev libbz2-1.0 libbz2-dev python3 python3-zmq linux-headers-$(uname -r) build-essential zlib1g-dev dkms libgsl-dev

                print_message "To use Node.js and NPM we need to install them separately"

                print_message "Downloading a setup script for Node.js 10.x"
                wget https://deb.nodesource.com/setup_10.x -O nodejs_setup10.x.sh

                print_message "Running the script"
                source nodejs_setup10.x.sh

                apt-get install nodejs
            elif expr 10 '<=' "$release" &>/dev/null
            then
                print_message "Installing required packages, for Debian 10 Buster..."
                apt-get install lsb-release vim git tmux clang libzmq5 libzmq3-dev libjsoncpp-dev libjsoncpp1 libjansson-dev libjansson4 zlib1g zlib1g-dev libbz2-1.0 libbz2-dev python3 python3-zmq nodejs npm linux-headers-$(uname -r) build-essential zlib1g-dev dkms libgsl-dev

                print_message "Updating NPM"
		npm install npm@latest -g
            else
                print_message "Unexpected ""$distribution"" version, you will have to install packages manually"
            fi
        else
            print_message "Unexpected distribution, you will have to install packages manually"
        fi
    elif [ "x""`command -v termux-info`""x" != "xx" ]
    then
        print_message "Termux system found!"

        print_message "Upgrading system..."
        apt update
        apt upgrade

        print_message "Installing required packages..."
        apt install vim git tmux clang libzmq libzmq-dev jsoncpp jsoncpp-dev libjansson libjansson-dev libbz2 libbz2-dev python gsl gsl-dev
    else
        print_message "ERROR: Unable to find lsb_release."
        echo "It can be installed with:"
        echo "On Fedora: dnf install redhat-lsb"
        echo "On Debian and Ubuntu: apt-get install lsb-release"
    fi
elif [ "$kernel_name" == "Darwin" ]
then
    print_message "macOS system found!"

    if [ "x""`command -v port`""x" != "xx" ]
    then
        print_message "Using MacPorts to install dependencies"
        print_message "Upgrading system..."
        port selfupdate
        port upgrade outdated

        print_message "Installing required packages, for macOS..."
        port install tmux zmq-devel jsoncpp-devel jansson python37 py37-zmq nodejs10 npm6 bzip2 zlib gsl
    else
        print_message "Unable to find MacPorts, you will have to install packages manually"
    fi
elif [ "$kernel_name" == "FreeBSD" ]
then
    print_message "FreeBSD system found!"
    print_message "WARNING: This system is not supported"

    print_message "Upgrading system..."
    pkg update
    pkg upgrade

    print_message "Installing required packages, for FreeBSD..."
    pkg install tmux libzmq4 jsoncpp jansson python35 node npm bzip2 lzlib gsl

    print_message "Creating symlink for python3..."
    ln -s `which python3.5` /usr/local/bin/python3

    print_message "Updating libraries list..."
    ldconfig
else
    print_message "Unexpected kernel, you will have to install packages manually"
fi
