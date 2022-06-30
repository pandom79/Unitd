#!/bin/sh -e

PATH=$PATH

[ -n "$VIRTUALIZATION" ] && exit 0

swapon -a
