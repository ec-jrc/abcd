#! /bin/bash

# Disable the RedPiata web-based interface
systemctl stop redpitaya_nginx.service
systemctl stop redpitaya_scpi
systemctl stop jupyter

# Configure the FPGA with the proper firmware
cat /opt/redpitaya/fpga/fpga_classic.bit > /dev/xdevcfg
