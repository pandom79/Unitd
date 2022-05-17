#!/bin/sh -e

if [ -z "$VIRTUALIZATION" ]; then
    /usr/sbin/seedrng
fi
