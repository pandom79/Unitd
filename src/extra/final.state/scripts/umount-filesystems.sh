#!/bin/sh -e

if [ -z "$VIRTUALIZATION" ]; then
    swapoff -a
    umount -r -a -t nosysfs,noproc,nodevtmpfs,notmpfs || true
    mount -o remount,ro / || true
fi

sync

