#! /bin/bash

#  (C) Copyright 2022, European Union, Cristiano Lino Fontana
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
    printf '#%.0s' $(seq -s' ' 0 ${length})
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
    printf '#%.0s' $(seq -s' ' 0 ${length})
    printf '\n'
    # Resetting all attributes
    printf '\e[0m'
}

kernel_name="$(uname -s)"

print_message "Kernel name: ${kernel_name}"

if [[ "${kernel_name}" != "Linux" ]]
then
    print_message "ERROR: Unexpected kernel, this system is not supported. You have to install packages manually."
else
    print_message "Linux system found!"

    installation_successfull=-1

    if [[ ! -n "$(command -v lsb_release)" ]]
    then
        print_message "ERROR: Unable to find the command: lsb_release"
        echo "It can be installed with:"
        echo "On Debian and Ubuntu: apt-get install lsb-release"
        echo "On CentOS and Fedora: dnf install redhat-lsb"
    else
        distribution="$(lsb_release -s -i)"
        release="$(lsb_release -s -r)"
        release_major="${release%.*}"

        print_message "Detected distribution: ${distribution}; major release: ${release_major}"

        if [[ "${distribution}" == "CentOS" || "${distribution}" == "Rocky" ]]
        then
            print_message "Upgrading system..."
            dnf upgrade

            if [[ "${release_major}" -eq 8 ]]
            then
                # The EPEL repository is for jsoncpp
                print_message "Enabling EPEL repository"

                ## The CentOS wiki suggests this, but for earlier releases:
                #dnf --enablerepo=extras install epel-release
                # The Fedora wiki suggests this:
                sudo dnf install https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm
                # This is needed for the EPEL repository, according to the Fedora wiki,
                # but it looks more like something for RHEL.
                #subscription-manager repos --enable "codeready-builder-for-rhel-8-*-rpms"
                sudo dnf config-manager --set-enabled PowerTools

                print_message "Upgrading again the system..."
                sudo dnf upgrade

                print_message "Installing required packages, for ${distribution}..."
                sudo dnf install redhat-lsb vim-enhanced git tmux clang jansson-devel jansson zlib zlib-devel bzip2-libs bzip2-devel nodejs npm kernel-devel kernel-headers elfutils-libelf-devel gsl gsl-devel jsoncpp jsoncpp-devel cmake zeromq zeromq-devel python38 python3-zmq python3-numpy python3-scipy python3-matplotlib wget dkms

                installation_successfull=$?
            else
                print_message "ERROR: Unexpected ${distribution} version, you will have to install packages manually"
            fi
        elif [ "${distribution}" == "Ubuntu" ]
        then
            print_message "Upgrading system..."
            sudo apt-get update
            sudo apt-get upgrade

            if [[ 20 -eq "${release_major}" ]]
            then
                print_message "Installing required packages, for Ubuntu 20 Focal Fossa..."
                sudo apt-get install lsb-release vim git tmux clang libzmq5 libzmq3-dev libjsoncpp-dev libjsoncpp1 libjansson-dev libjansson4 zlib1g zlib1g-dev libbz2-1.0 libbz2-dev python3 python3-zmq python3-numpy python3-scipy python3-matplotlib nodejs npm linux-headers-generic build-essential dkms libgsl-dev

                installation_successfull=$?
            elif [[ 22 -eq "${release_major}" ]]
            then
                print_message "Installing required packages, for Ubuntu 22 Jammy Jellyfish..."
                sudo apt-get install lsb-release vim git tmux clang libc++-dev libc++abi-dev libzmq5 libzmq3-dev libjsoncpp-dev libjsoncpp25 libjansson-dev libjansson4 zlib1g zlib1g-dev libbz2-1.0 libbz2-dev python3 python3-zmq python3-numpy python3-scipy python3-matplotlib nodejs npm linux-headers-generic build-essential dkms libgsl-dev

                installation_successfull=$?
            else
                print_message "ERROR: Unexpected ${distribution} version, you will have to install packages manually"
            fi
        else
            print_message "ERROR: Unexpected ${distribution}, you will have to install packages manually"
        fi
    fi

    if [[ ${installation_successfull} -eq 0 ]]
    then
        print_message "The required packages were installed."

        print_message "Compiling ABCD..."
        make

        print_message "Installing the dependencies of node.js..."
        if [[ ! -d "wit" ]]
        then
            print_message "ERROR: Unable to locate wit folder. Was the installation script run in the ABCD main folder?"
        else
            cd wit
            npm install

            if [[ $? -eq 0 ]]
            then
                print_message "Installation successfull!"
            else
                print_message "ERROR unable to install ABCD"
            fi
        fi
    fi
fi
