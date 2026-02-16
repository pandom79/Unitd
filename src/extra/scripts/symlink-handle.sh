#!/bin/sh -e

# Create and remove symlink
# $1 = operation (add or remove)
# $2 = from
# $3 = to
OPERATION="$1"
FROM="$2"
TO="$3"

if [ $OPERATION == "remove" ]; then
	rm -rf $TO || true
elif [ $OPERATION == "add" ]; then
	rm -rf $TO || true
	ln -s $FROM $TO
fi
