#!/bin/sh
# PURPOSE: Keep the date/time moving forward from the last known date.

tk="/home/pi/radioplay/vars/timekeep.txt"
main() {
	date=$(cat "$tk" 2>/dev/null);
	if [ "$date" != "" ]; then
		echo "Previously set date: $date."
		sudo date -s "$date"
	else
		echo "No previously set date."
	fi
	while [ true ]; do
		date=$(date +"%D %T")
		echo "$date" >"$tk"
		/sbin/fake-hwclock save # Save current time in case of power failure
		sleep 1
	done
}

main
