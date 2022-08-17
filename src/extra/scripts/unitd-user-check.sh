#!/bin/sh -e

export PATH=$PATH

USER_STATE="user.state"
# $1 = UNITS_USER_LOCAL_PATH ($HOME/.config/unitd/units)
# $2 = UNITS_USER_ENAB_PATH ($HOME/.local/share/unitd/units/user.state)
# $3 = USER UID
# $4 = USER NAME
UNITS_USER_LOCAL_PATH="$1"
UNITS_USER_ENAB_PATH="$2"
USER_UID="$3"
USER_NAME="$4"

# Check UNITS_USER_LOCAL_PATH
if [ ! -d "$UNITS_USER_LOCAL_PATH" ]; then
    mkdir -p "$UNITS_USER_LOCAL_PATH"
    chmod 0755 -R "$UNITS_USER_LOCAL_PATH"
fi

# Check UNITS_USER_ENAB_PATH
UNITS_USER_ENAB_PATH="$UNITS_USER_ENAB_PATH/$USER_STATE"
if [ ! -d "$UNITS_USER_ENAB_PATH" ]; then
    mkdir -p "$UNITS_USER_ENAB_PATH"
    chmod 0755 -R "$UNITS_USER_ENAB_PATH"
fi

# Check the instance is not already running for the user.
# Please note that if we run the instance under valgrind supervision then this check fails!!
NUM_INSTANCES=$(pgrep -l -u "$USER_UID" -U "$USER_UID" -x unitd | wc -l)
if [ $NUM_INSTANCES -gt 1 ]; then
    exit 114
fi
