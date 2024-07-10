#!/bin/sh -e

export PATH=$PATH

[ -n "$VIRTUALIZATION" ] && exit 0
# Do not try to load modules if kernel does not support them.
[ ! -e /proc/modules ] && exit 0

$UNITD_DATA_PATH/scripts/modules-load -v | tr '\n' ' ' | sed 's:insmod [^ ]*/::g; s:\.ko\(\.gz\)\? ::g'
