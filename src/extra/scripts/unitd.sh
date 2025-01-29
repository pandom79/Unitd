#!/bin/sh -e

export PATH=$PATH

# We only have one argument for this script (operation).
# The other data will be set as environment variables.

# $1 = operation
OPERATION="$1"

case $OPERATION in
"virtualization")
    # Detect LXC (and other) containers
    [ -z "${container+x}" ] || exit 1
    [ ! -e /.dockerenv ] || exit 1
    ;;
"cat-unit")
    cat $UNIT_PATH
    ;;
"edit-unit")
    . $UNITD_DATA_PATH/functions/functions
    if command -v vim >/dev/null; then
        vim $UNIT_PATH
    elif command -v vi >/dev/null; then
        vi $UNIT_PATH
    else
        msg_error "Please, install 'vim' or 'vi' text editor."
    fi
    ;;
"send-wallmsg")
    wall $MSG
    ;;
"save-time")
    rm -rf $PATTERN || true
    touch $FILE_PATH
    ;;
"remove")
    rm -rf $PATTERN || true
    ;;
"default-syml")
    rm -rf $TO || true
    ln -s $FROM $TO
    ;;
esac
