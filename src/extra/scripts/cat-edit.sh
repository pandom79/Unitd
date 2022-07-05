#!/bin/sh -e

export PATH=$PATH

. $UNITD_DATA_PATH/functions/functions

# Show or edit the unit content
# $1 = operation (cat or edit)
# $2 = unit path
OPERATION="$1"
UNIT_PATH="$2"

if [ $OPERATION == "cat" ]; then
    cat $UNIT_PATH
elif [ $OPERATION == "edit" ]; then
    if command -v vim > /dev/null; then
        vim $UNIT_PATH
    elif command -v vi > /dev/null; then
        vi $UNIT_PATH
    else
        msg_error "Please, install 'vim' or 'vi' text editor."
    fi
fi
