while [ true ]; do
	echo $(date +"%Y-%m-%d %H:%M:%S") $(for proc in play3abn mplay.bin mdown src-3abn lrucache mscreen; do pid=$(pidof $proc); echo -n " $proc" $(cat /proc/$pid/status 2>/dev/null | grep VmSize | sed -e 's/VmSize://g' -e 's/kB/ /g'); done) " " $(echo /tmp/play3abn/tmp/playtmp-* | sed 's/.*playtmp-//g' | cut -b 24-)
sleep 10
done
