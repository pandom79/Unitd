#!/bin/sh -e

export PATH=$PATH

# Configure kernel parameters:
if [ -x /sbin/sysctl ]; then
    sysctl -e --system >/dev/null
fi
