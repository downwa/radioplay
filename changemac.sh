#!/bin/sh

fixmac() {
	local cmdline=$1
	# If clone has same MAC address as mine (or none) then change it.
	clone=$(grep " smsc95xx.macaddr=" "$cmdline" | sed -e 's/.* smsc95xx.macaddr=//g')
	mymac=$(ifconfig eth0 | grep HWaddr | awk '{print $5}')
	[ "$clone" != "" -a "$clone" != "$mymac" ] && return

	mac=$(dd if=/dev/urandom bs=3 count=1 2>/dev/null | hexdump -e '3/1 ":%02x"')
	mac="b8:27:eb$mac"

	echo "Setting MAC to $mac"
	sed -i "$cmdline" -e "s/$/ smsc95xx.macaddr=$mac/g" -e "s/ smsc95xx.macaddr=.*/ smsc95xx.macaddr=$mac/g"
}

[ "$1" = "" ] && echo "Missing path to /boot/cmdline.txt" 1>&2 && exit 1
fixmac "$1"
