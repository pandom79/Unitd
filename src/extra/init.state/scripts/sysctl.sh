#!/bin/sh -e

# Configure kernel parameters:
if [ -x /sbin/sysctl -a -z "$VIRTUALIZATION" ]; then
  /sbin/sysctl -e --system > /dev/null || true
fi
