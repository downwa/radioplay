#!/bin/sh

progress() {
	[ "$odate" = "0" ] && return
	date=$(date +%s)
	pid=$(pidof cp)
	[ "$pid" = "" ] && pid=$(pidof rsync)
	[ "$pid" = "" ] && return

	dt=$((date-odate))

# 	free=$(echo $(df -h /mnt/tgt/* | egrep -v "^Filesystem|^/dev/root" | sort | uniq | sed -e 's@^/dev/@@g' -e 's/%/%%/g' | awk '{print $1"-"$5}'))

	src=$(strings /proc/$pid/cmdline | tail -n 2 | head -n 1)
	dst=$(strings /proc/$pid/cmdline | tail -n 2 | tail -n 1)
	srcu=$(df "$src" | grep -v "^Filesystem" | awk '{print $3}')
	dstu=$(df "$dst" | grep -v "^Filesystem" | awk '{print $3}')
	pct=$((dstu*100/srcu))

	kps=$(((dstu-odstu)/$dt))
	[ "$kps" = "0" ] && return
	kleft=$((srcu-dstu))
	tleft=$((kleft/kps))
	hh=$((tleft/3600))
	sec=$((tleft%3600))
	mm=$((sec/60))
	ss=$((sec%60))
	# e.g. 44% (~1:01:28) 
	printf "%s%% (~%d:%02d:%02d)\n" "$pct" "$hh" "$mm" "$ss"
}

main() {
	odate=0
	while [ true ]; do
		progress >/tmp/progress.txt.tmp
		mv /tmp/progress.txt.tmp /tmp/progress.txt
		odate=$date
		odstu=$dstu
		sleep 10
	done
}

>/tmp/progress.txt
main 2>/tmp/progress.err
