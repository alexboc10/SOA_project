#! /bin/bash

make
sudo insmod TBDE.ko

# 'c' for char device
sudo mknod /dev/TBDE c 238 0
