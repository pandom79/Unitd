#!/bin/sh -e

export PATH=$PATH

if [ -z "$VIRTUALIZATION" ]; then
	udevadm control --exit
fi
