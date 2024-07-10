#!/bin/sh -e

export PATH=$PATH

[ -n "$VIRTUALIZATION" ] && exit 0

swapon -a
