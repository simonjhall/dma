#!/bin/bash
cd "$(dirname "$0")"

sudo rmmod dmaer_master
sudo insmod dmaer_master.ko || exit 1
sudo rm -f /dev/dmaer_4k
major=$(awk '$2=="dmaer" {print $1}' /proc/devices)
echo device $major
sudo mknod /dev/dmaer_4k c $major 0
