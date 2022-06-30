#!/bin/sh -e

PATH=$PATH

[ -n "$VIRTUALIZATION" ] && exit 0

if [ -x /usr/lib/systemd/systemd-udevd ]; then
    _udevd=/usr/lib/systemd/systemd-udevd
elif [ -x /sbin/udevd -o -x /bin/udevd ]; then
    _udevd=udevd
fi

if [ -n "${_udevd}" ]; then
    ${_udevd} --daemon
    udevadm trigger --action=add --type=subsystems
    udevadm trigger --action=add --type=devices
#    udevadm settle
fi
