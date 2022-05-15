#!/bin/sh -e

# Configure kernel parameters:
if [ -x /sbin/sysctl -a -r /etc/sysctl.conf -a -z "$VIRTUALIZATION" ]; then
  /sbin/sysctl -e --system || true
elif [ -x /sbin/sysctl -a -z "$VIRTUALIZATION" ]; then
  # Don't say "Applying /etc/sysctl.conf" or complain if the file doesn't exist
  /sbin/sysctl -e --system 2> /dev/null || true
fi

