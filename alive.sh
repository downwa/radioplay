#!/bin/sh
# Let server know we are still alive

pid=$(pidof -o %PPID -x alive.sh)
[ "$pid" != "" ] && exit

while [ true ]; do
	ping=$(./ping.sh 2>&1)
	date +"%D %T: $ping" >/tmp/ping.log
	grep -q ^Reboot /tmp/rmtcmd.txt && reboot
	grep -q ^Poweroff /tmp/rmtcmd.txt && poweroff
	sleep 1 # But ping should wait up to 60 seconds unless events are waiting
done
