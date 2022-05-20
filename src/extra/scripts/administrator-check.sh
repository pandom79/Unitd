#!/bin/sh

export PATH=/sbin:/usr/sbin:/bin:/usr/bin:

. $UNITD_DATA_PATH/functions/functions

#Get params
PARAMS="$@"

if command -v pkexec > /dev/null; then
    pkexec $PARAMS
elif command -v sudo > /dev/null; then
    msg_warn "Authentication required for 'unitctl'."
    sudo -k $PARAMS
else
    msg_error "Please, run this program as administrator."
fi

