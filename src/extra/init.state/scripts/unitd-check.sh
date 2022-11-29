#!/bin/sh -e

export PATH=$PATH
DEF_STATE="multi-user-net.state"
DEF_SYML="default.state"
STATES="custom.state graphical.state multi-user-net.state multi-user.state poweroff.state reboot.state single-user.state"
VALID_STATES="custom.state graphical.state multi-user-net.state multi-user.state single-user.state"

reset_symlink() {
    rm -rf "$UNITS_ENAB_PATH/$DEF_SYML" || true
    ln -s "$UNITS_ENAB_PATH/$DEF_STATE" "$UNITS_ENAB_PATH/$DEF_SYML"
}

# Check UNITS_PATH
if [ ! -d "$UNITS_PATH" ]; then
    mkdir -p "$UNITS_PATH"
    chmod 0755 -R "$UNITS_PATH"
fi

# Check UNITS_USER_PATH
if [ ! -d "$UNITS_USER_PATH" ]; then
    mkdir -p "$UNITS_USER_PATH"
    # The 'users' group must have write permission to allow 'unitctl edit' command (user instance)
    # to properly work.
    chown :users -R "$UNITS_USER_PATH"
    chmod 0775 -R "$UNITS_USER_PATH"
fi

# Check UNITS_ENAB_PATH
if [ ! -d "$UNITS_ENAB_PATH" ]; then
    mkdir -p "$UNITS_ENAB_PATH"
    chmod 0755 -R "$UNITS_ENAB_PATH"
fi

# Check UNITD_TIMER_DATA_PATH
if [ ! -d "$UNITD_TIMER_DATA_PATH" ]; then
    mkdir -p "$UNITD_TIMER_DATA_PATH"
    chmod 0755 -R "$UNITD_TIMER_DATA_PATH"
fi

# Check the states folders
for STATE in ${STATES[@]}
do
    STATE_PATH="$UNITS_ENAB_PATH/$STATE"
    if [ ! -d "$STATE_PATH" ]; then
        mkdir -p "$STATE_PATH"
        chmod 0755 -R "$STATE_PATH"
    fi
done

# Check default symlink
POINTS_TO=$(readlink "$UNITS_ENAB_PATH/$DEF_SYML" || true)
if [ -z "$POINTS_TO" ]; then
    reset_symlink
else
    # Check where it points
    FOUND=0
    for STATE in ${VALID_STATES[@]}
    do
        # Compare with absolute or relative state path
        if [ "$POINTS_TO" == "$STATE" -o "$POINTS_TO" == "$UNITS_ENAB_PATH/$STATE" ]; then
            FOUND=1
            break
        fi
    done
    if [ $FOUND -ne 1 ]; then
        reset_symlink
    fi
fi
