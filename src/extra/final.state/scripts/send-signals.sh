#!/bin/sh -e

pkill --inverse -s0,1 -TERM
sleep 1
pkill --inverse -s0,1 -KILL

