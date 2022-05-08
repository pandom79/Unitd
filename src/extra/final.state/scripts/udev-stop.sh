#!/bin/sh -e

if [ -z "$VIRTUALIZATION" ]; then
    udevadm control --exit
fi
