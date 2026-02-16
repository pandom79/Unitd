#!/bin/sh -e

export PATH=$PATH

if [ -z "$VIRTUALIZATION" ]; then
	swapoff -a
	umount -r -a -t nosysfs,noproc,nodevtmpfs,notmpfs
	LIBMOUNT_FORCE_MOUNT2=always mount -o remount,ro /
fi

sync
