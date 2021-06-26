#! /bin/bash

make
sudo insmod TBDE.ko

# "c" as char device
sudo mknod /dev/TBDE c 237 0

sudo dmesg | grep TBDE
