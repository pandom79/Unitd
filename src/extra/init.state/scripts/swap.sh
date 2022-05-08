#!/bin/sh -e

[ -n "$VIRTUALIZATION" ] && exit 0

swapon -a
