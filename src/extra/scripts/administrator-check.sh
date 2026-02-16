#!/bin/sh

export PATH=$PATH

. $UNITD_DATA_PATH/functions/functions

#Get params
PARAMS="$@"

if command -v pkexec >/dev/null; then
	pkexec $PARAMS
elif command -v sudo >/dev/null; then
	msg_warn "Authentication required for '$1'."
	sudo -k $PARAMS
elif command -v doas >/dev/null; then
	msg_warn "Authentication required for '$1'."
	doas $PARAMS
else
	msg_error "Please, run this program as administrator."
fi
