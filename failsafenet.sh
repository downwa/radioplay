#!/bin/sh

sleep 10
while [ true ]; do
	ip=$(ifconfig eth0 | grep "inet addr:" | awk '{print $2}' | cut -d ':' -f 2)
	if [ "$ip" = "" -o "$ip" = "0.0.0.0" ]; then
		ifconfig eth0 192.168.42.43		
	fi
	/sbin/fake-hwclock save # Save current time in case of power failure
	sleep 5
done
