#! /bin/bash

make
sudo insmod tag_service.ko
sudo dmesg | grep "TAG_SERVICE"
