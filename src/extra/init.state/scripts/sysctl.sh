#!/bin/sh -e

if [ -x /sbin/sysctl -o -x /bin/sysctl ]; then
    for i in /etc/sysctl.d/*.conf; do
        sysctl -p "$i" > /dev/null
    done
    if [ -r /etc/sysctl.conf ]; then
        sysctl -p /etc/sysctl.conf > /dev/null
    fi
fi
