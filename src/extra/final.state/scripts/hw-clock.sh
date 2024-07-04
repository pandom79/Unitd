#!/bin/sh -e

export PATH=$PATH

. $UNITD_CONF_PATH/unitd.conf

if [ -z "$VIRTUALIZATION" -a ! -z "$HARDWARECLOCK" ]; then
    hwclock --systohc ${HARDWARECLOCK:+--$(echo $HARDWARECLOCK | tr A-Z a-z)} || true
fi
