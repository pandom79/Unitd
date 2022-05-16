#!/bin/sh -e

. $UNITD_DATA_PATH/functions/functions

# Show or edit the unit content
# $1 = operation (cat or edit)
# $2 = unit path
OPERATION="$1"
UNIT_PATH="$2"

if [ $OPERATION == "cat" ]; then
    /usr/bin/cat $UNIT_PATH
elif [ $OPERATION == "edit" ]; then
    if [ -x /usr/bin/vim ]; then
        /usr/bin/vim $UNIT_PATH
    elif [ -x /usr/bin/vi ]; then
        /usr/bin/vi $UNIT_PATH
    else
        msg_error "Unable to find 'vim' or 'vi' text editor!"
    fi
fi
