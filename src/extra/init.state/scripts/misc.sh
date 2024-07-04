#!/bin/sh -e

export PATH=$PATH

. $UNITD_CONF_PATH/unitd.conf

install -m0664 -o root -g utmp /dev/null $OUR_UTMP_FILE
unitctl -o # Write a utmp/wtmp 'reboot' record

if [ -z "$VIRTUALIZATION" ]; then
    seedrng >/dev/null
fi

ip link set up dev lo

[ -r /etc/hostname ] && read -r HOSTNAME </etc/hostname
if [ ! -z "$HOSTNAME" ]; then
    printf "%s" "$HOSTNAME" >/proc/sys/kernel/hostname
fi

if [ ! -z "$TIMEZONE" ]; then
    ln -sf "/usr/share/zoneinfo/$TIMEZONE" /etc/localtime
fi
