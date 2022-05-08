#!/bin/sh -e

# Detect LXC containers
[ ! -e /proc/self/environ ] && exit 0
if grep -q lxc /proc/self/environ >/dev/null; then
    exit 1
fi
exit 0
