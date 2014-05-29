#!/bin/sh
# Let server know we are still alive

pid=$(pidof -o %PPID -x alive.sh)
[ "$pid" != "" ] && exit

cd "/home/pi/radioplay/"

while [ true ]; do
	date +"%D %T: pinging..." >/tmp/ping.log
	pids=$(ps waxf | grep ping | grep -v grep | awk '{print $1}')
	[ "$pids" != "" ] && kill $pids 2>/dev/null
	./ping.sh >>/tmp/ping.log
	date +"%D %T: pinged." >>/tmp/ping.log
	grep -q ^Reboot /tmp/rmtcmd.txt && reboot
	grep -q ^Poweroff /tmp/rmtcmd.txt && poweroff
	sleep 1 # But ping should wait up to 60 seconds unless events are waiting
done
