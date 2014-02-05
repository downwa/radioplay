#!/bin/sh

while read line; do
	dt=${line:0:19}
	tt=${line:14:2}
	len=${line:20:4}
	rest=${line:25}
	if [ "$len" -le 0900 ]; then
		if [ "$len" -le 0015 -a "$rest" = "StationID" ]; then
			len="0015"
		else
			len="0900"
			[ "$tt" = "45" ] && len="0885"
		fi
	else
		if [ "$len" -le 1800 ]; then
			len=1800
			[ "$tt" = "30" ] && len="1785"
		else
			len=3585
		fi
	fi
	echo "$dt $len $rest"
done
