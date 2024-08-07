#!/bin/sh -e

export PATH=$PATH

if [ ! -e "$OUR_WTMP_FILE" ]; then
    install -m0664 -o root -g utmp /dev/null "$OUR_WTMP_FILE"
fi

if [ ! -e /var/log/btmp ]; then
    install -m0600 -o root -g utmp /dev/null /var/log/btmp
fi
if [ ! -e /var/log/lastlog ]; then
    install -m0600 -o root -g utmp /dev/null /var/log/lastlog
fi

install -dm1777 /tmp/.X11-unix /tmp/.ICE-unix
rm -f /etc/nologin /forcefsck /forcequotacheck /fastboot
