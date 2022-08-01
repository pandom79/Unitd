#!/bin/sh -e

export PATH=$PATH

# Create the user dirs
# $1 = UNITS_USER_LOCAL_PATH (/.config/unitd/units)
# $2 = UNITS_USER_ENAB_PATH (/.local/share/unitd/)
UNITS_USER_LOCAL_PATH="$1"
UNITS_USER_ENAB_PATH="$2"

# Check UNITS_USER_LOCAL_PATH
if [ ! -d "$UNITS_USER_LOCAL_PATH" ]; then
    mkdir -p "$UNITS_USER_LOCAL_PATH"
    chmod 0755 -R "$UNITS_USER_LOCAL_PATH"
fi

# Check UNITS_USER_ENAB_PATH
if [ ! -d "$UNITS_USER_ENAB_PATH" ]; then
    mkdir -p "$UNITS_USER_ENAB_PATH"
    chmod 0755 -R "$UNITS_USER_ENAB_PATH"
fi
