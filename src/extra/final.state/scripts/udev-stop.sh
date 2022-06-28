#!/bin/sh -e

if [ -z "$VIRTUALIZATION" ]; then
    /sbin/udevadm control --exit
fi
