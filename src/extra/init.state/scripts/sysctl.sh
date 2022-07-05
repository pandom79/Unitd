#!/bin/sh -e

export PATH=$PATH

# Configure kernel parameters:
if [ -x /sbin/sysctl -a -z "$VIRTUALIZATION" ]; then
  sysctl -e --system > /dev/null || true
fi
