#!/bin/sh

find /tmp/ping.log -cmin +2 -exec rm {} \;
killall alive.sh
if [ ! -f /tmp/ping.log ]; then
	touch /tmp/ping.log
	nohup ./alive.sh 2>/dev/null >/dev/null &
fi
