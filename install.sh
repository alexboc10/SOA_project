#! /bin/bash

make
sudo insmod TBDE.ko

# "c" as char device
# 238 as major number
sudo mknod /dev/TBDE c 238 0

sudo dmesg | grep TBDE
