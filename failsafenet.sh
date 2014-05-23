#!/bin/sh

sleep 10
while [ true ]; do
	ip=$(ifconfig eth0 | grep "inet addr:" | awk '{print $2}' | cut -d ':' -f 2)
	if [ "$ip" = "" -o "$ip" = "0.0.0.0" ]; then
		#ifconfig eth0 192.168.42.43
		ifup -a
	fi
	sleep 5
done
