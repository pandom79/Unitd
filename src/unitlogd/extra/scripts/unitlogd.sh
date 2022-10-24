#!/bin/sh -e

export PATH=$PATH

# We only have one argument for this script (operation).
# The other data will be set as environment variables.

# $1 = operation
OPERATION="$1"

case $OPERATION in
    "create")
        if [ ! -d "$UNITLOGD_PATH" ]; then
            mkdir -p "$UNITLOGD_PATH"
        fi
        if [ ! -f "$UNITLOGD_LOG_PATH" ]; then
            touch "$UNITLOGD_LOG_PATH"
        fi
        if [ ! -f "$UNITLOGD_INDEX_PATH" ]; then
            touch "$UNITLOGD_INDEX_PATH"
        fi
        if [ ! -f "$UNITLOGD_LOCK_PATH" ]; then
            touch "$UNITLOGD_LOCK_PATH"
        fi
        chmod -R 0650 "$UNITLOGD_PATH"
        chown -R :users "$UNITLOGD_PATH"
    ;;
    "follow")
        tail -f "$UNITLOGD_LOG_PATH"
    ;;
    "create-index")
        rm -rf "$UNITLOGD_INDEX_PATH" || true
        touch "$UNITLOGD_INDEX_PATH"
        chmod 0650 "$UNITLOGD_INDEX_PATH"
        chown :users "$UNITLOGD_INDEX_PATH"
    ;;
    "create-tmp-log")
        tmp_log="$UNITLOGD_LOG_PATH"$TMP_SUFFIX
        touch "$tmp_log"
        chmod 0650 "$tmp_log"
        chown :users "$tmp_log"
    ;;
    "ren-tmp-log")
        log="$UNITLOGD_LOG_PATH"
        tmp_log="$log"$TMP_SUFFIX
        rm -rf "$log"
        mv "$tmp_log" "$log"
    ;;
esac;
