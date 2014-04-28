#!/bin/sh

find /tmp/play3abn/tmp/playq/ -type l -printf "%f %l\n" | sort | while read date time tgt; do
	url=$(echo "$tgt" | sed -e 's/.*;url=//g' -e 's/;.*//g' -e 's/%/\\x/g')
	url=$(printf "$url")
	[ ! -f "$url" ] && url="MISSING: $url"
	echo "$date $time $url"
done
