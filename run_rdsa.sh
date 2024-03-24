#!/bin/bash
make clean
make
insmod mymodule.ko
dmesg -c
gcc -o put put.c
#./put DEADBEEF a.txt
#rmmod mymodule.ko
#dmesg -c