#! /bin/bash

sudo rmmod tag_service
make clean
sudo dmesg | grep "TAG_SERVICE"
