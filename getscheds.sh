#!/bin/sh

rm -f /tmp/play3abn/scheds
mkfifo /tmp/play3abn/scheds
./getsched 2>/tmp/play3abn/curlogs/getscheds.err </tmp/play3abn/scheds
