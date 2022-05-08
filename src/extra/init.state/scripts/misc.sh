#!/bin/sh -e

if [ -z "$VIRTUALIZATION" ]; then
    cp /var/lib/random-seed /dev/urandom >/dev/null 2>&1 || true
fi

ip link set up dev lo

[ -r /etc/hostname ] && read -r HOSTNAME < /etc/hostname
if [ -n "$HOSTNAME" ]; then
    printf "%s" "$HOSTNAME" > /proc/sys/kernel/hostname
fi

if [ -n "$TIMEZONE" ]; then
    ln -sf "/usr/share/zoneinfo/$TIMEZONE" /etc/localtime
fi
