#!/bin/sh -e

PATH=$PATH

. $UNITD_DATA_PATH/functions/functions

if [ -z "$VIRTUALIZATION" ]; then
    deactivate_vgs
    deactivate_crypt
fi
