#!/bin/sh -e

export PATH=$PATH

[ ! -z "$VIRTUALIZATION" ] && exit 0

swapon -a
