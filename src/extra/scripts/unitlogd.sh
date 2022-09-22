#!/bin/sh -e

export PATH=$PATH

# We only have one argument for this script (operation).
# The other data will be set as environment variables.

# $1 = operation
OPERATION="$1"

case $OPERATION in
        "init")
            if [ ! -d "$UNITLOGD_PATH" ]; then
                mkdir -p "$UNITLOGD_PATH"
                chmod -R 0644 "$UNITLOGD_PATH"
            fi
            if [ ! -f "$UNITLOGD_LOG_PATH" ]; then
                touch "$UNITLOGD_LOG_PATH"
            fi
            if [ ! -f "$UNITLOGD_INDEX_PATH" ]; then
                touch "$UNITLOGD_INDEX_PATH"
            fi
            if [ ! -f "$UNITLOGD_BOOT_LOG_PATH" ]; then
                touch "$UNITLOGD_BOOT_LOG_PATH"
            fi
        ;;
esac;
