#!/bin/sh

sleep 10
while [ true ]; do
	ip=$(ifconfig eth0 | grep "inet addr:" | awk '{print $2}' | cut -d ':' -f 2)
	if [ "$ip" = "" -o "$ip" = "0.0.0.0" ]; then
		#ifconfig eth0 192.168.42.43
		ifup -a
		ip=$(ifconfig eth0 | grep "inet addr:" | awk '{print $2}' | cut -d ':' -f 2)
		if [ "$ip" = "" ]; then
			ip=$(cat /var/lib/dhcp/dhclient.eth0.leases | grep address | tail -n 1 | awk '{print $2}' | cut -d ';' -f 1)
			[ "$ip" != "" ] && ifconfig eth0 $ip
		fi
		gw=$(cat /var/lib/dhcp/dhclient.eth0.leases | grep routers | tail -n 1 | awk '{print $3}' | cut -d ';' -f 1)
		if [ "$gw" != "" ]; then
			route del default
			route add default gw $gw
		fi
	fi
	sleep 5
done
