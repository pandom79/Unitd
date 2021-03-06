#!/bin/sh -e

export PATH=$PATH

if [ -z "$VIRTUALIZATION" ]; then
    swapoff -a
    umount -r -a -t nosysfs,noproc,nodevtmpfs,notmpfs
    mount -o remount,ro /
fi

sync

