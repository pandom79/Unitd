#!/bin/sh -e

PATH=$PATH

if [ -z "$VIRTUALIZATION" ]; then
    udevadm control --exit
fi
