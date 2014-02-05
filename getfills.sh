#!/bin/sh

rm -f /tmp/play3abn/fills
mkfifo /tmp/play3abn/fills
./getfill 2>/tmp/play3abn/curlogs/getfill.err </tmp/play3abn/fills
