#!/bin/sh -e

export PATH=$PATH

# Send msg to all users
# $1 = msg
MSG="$1"

wall $MSG
