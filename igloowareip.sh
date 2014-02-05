#!/bin/sh

domain="radio.iglooware.com" # iglooware.ath.cx
vars="/tmp/play3abn/vars"
logs="/tmp/play3abn/curlogs"

ips() {
if=$(route -n | grep ^0.0.0.0 | awk '{print $8}'); ifconfig "$if" | grep HWaddr | awk '{print $5}' >$vars/macaddr.txt
nslookup "$domain" | grep Address | tail -n 1 | cut -d ':' -f 2 | awk '{print $1}' >$vars/igloowareip.txt
lynx -dump http://automation.whatismyip.com/n09230945.asp 2>"$logs/getpublicip.log" | awk '{print $1}' | head -n 1 >$vars/publicip.txt
}

pidcount=$(pidof -x igloowareip.sh 2>/dev/null | wc -w || pidof igloowareip.sh | wc -w)
[ "$pidcount" -gt "2" ] && echo "igloowareip.sh already running" 1>&2 && exit
ips &
