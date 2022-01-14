#!/usr/bin/env bash

modpoll -b 9600 -m rtu -a $1 -t 4 -c 7 -r 50 -p none -4 1 $2 $(date +'%Y') $(date +'%m') $(date +'%d') $(date +'%H') $(date +'%M') $(date +'%S') $(date +'%w')
modpoll -b 9600 -m rtu -a $1 -t 4:int -c 1 -r 57 -p none -4 1 $2 $(date +"%s")
modpoll -b 9600 -m rtu -a $1 -t 4 -c 1 -r 56 -p none -4 1 $2 $(date +"%w")
modpoll -b 9600 -m rtu -a $1 -t 4 -c 7 -r 50 -p none -4 1 -1 $2

REMOTE_TIME=$(modpoll -b 9600 -m rtu -a $1 -t 4:int -c 1 -r 57 -p none -4 1 -1 $2 | grep -o -P '(?<!\d)\d{10}(?!\d)')
LOCAL_TIME=$(date +%s)

echo Local time offset $[LOCAL_TIME - REMOTE_TIME] seconds

