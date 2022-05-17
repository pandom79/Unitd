#!/bin/sh -e

# Detect LXC (and other) containers
[ -z "${container+x}" ] || exit 1
