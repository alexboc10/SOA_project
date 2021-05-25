#! /bin/bash

make
sudo insmod TBDE.ko
sudo dmesg
