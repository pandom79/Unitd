#!/bin/sh -e

PATH=$PATH

if [ -z "$VIRTUALIZATION" ]; then
    seedrng > /dev/null
fi
