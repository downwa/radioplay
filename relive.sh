#!/bin/sh

find /tmp/ping.log -cmin +2 -exec rm {} \;
if [ ! -f /tmp/ping.log ]; then
	killall alive.sh
	touch /tmp/ping.log
	nohup /home/pi/radioplay/alive.sh 2>/tmp/alive.err >/dev/null &
fi
