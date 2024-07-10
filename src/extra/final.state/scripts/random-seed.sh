#!/bin/sh -e

export PATH=$PATH

if [ -z "$VIRTUALIZATION" ]; then
    seedrng >/dev/null || true
fi
