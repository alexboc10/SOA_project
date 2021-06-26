#! /bin/bash

sudo rmmod TBDE
make clean

sudo rm /dev/TBDE

sudo dmesg | grep TBDE
