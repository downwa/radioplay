#!/bin/sh

ping() {
	dev=$(route -n | grep "^0.0.0.0" | awk '{print $8}')
	mac=$(ifconfig "$dev" | grep HWaddr | awk '{print $5}')
	release=$(cat /home/pi/radioplay/vars/release.txt)
	playsec=$(cat /tmp/play3abn/vars/playsec.txt)
	playlen=$(cat /tmp/play3abn/vars/curplaylen.txt)
	playfile=$(cat /tmp/play3abn/vars/playfile.txt | cut -d ' ' -f 2- | sed -e 's/ /%20/g' -e 's/&/%26/g' -e 's/\?/%3f/g' -e 's/=/%3d/g')
	myip=$(cat /tmp/play3abn/vars/myip.txt)
	syncradio=$(cat /tmp/play3abn/vars/syncradio.txt | sed -e 's/ /%20/g' -e 's/&/%26/g' -e 's/\?/%3f/g' -e 's/=/%3d/g')
	scrnum=$(cat /tmp/play3abn/vars/scrnum.txt)
	syslog=$(tail -n 1 /var/log/syslog | sed -e 's/ /%20/g' -e 's/&/%26/g' -e 's/\?/%3f/g' -e 's/=/%3d/g')
	uptime=$(uptime | sed -e 's/ /%20/g')
	url="https://radio.iglooware.com/ping.php?mac=$mac&release=$release&playsec=$playsec&playlen=$playlen"
	url="$url&playfile=$playfile&myip=$myip&syncradio=$syncradio&scrnum=$scrnum&syslog=$syslog&uptime=$uptime"
	ppid=$$
	touch /tmp/ping-notify.flag
	>/tmp/rmtcmd.txt
	(curl --ipv4 --insecure --output /tmp/rmtcmd.txt "$url"; cat /tmp/rmtcmd.txt; touch /tmp/ping-notify.flag) &
	pid=$!
	inotifywait --timeout 60 -e attrib --quiet --quiet "/tmp/ping-notify.flag"
	kill $pid 2>/dev/null
}

ping 2>/dev/null
