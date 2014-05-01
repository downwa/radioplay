#!/bin/sh

sudo mkdir -p /media/RadioSD
mcount=$(mount | grep /RadioSD | wc -l)
[ "$mcount" -lt 1 ] && sudo mount --bind /RadioSD /media/RadioSD
sbin="$HOME/radioplay/x86_64"

perms() {
	while [ true ]; do
		sudo chmod 770 /tmp/play3abn/
		sudo chown -R $USER.$USER /tmp/play3abn/ 2>/dev/null
		sleep 5
		pidof play3abn >/dev/null || exit
	done
}

control_c() {
  kill $PID
  while [ true ]; do mcount=$(mount | grep /RadioSD | wc -l); [ "$mcount" -gt 1 ] && { sudo umount /media/RadioSD; continue; }; break; done
  exit $?
}
 

sudo rm -f /tmp/play3abn/play3abn.log
cd "$sbin"
#for file in lib*.so libs/lib*.so*; do
for file in lib*.so; do
	base=$(basename "$file")
	[ "$file" -nt /lib/i386-linux-gnu/$base ] && sudo cp -av "$file" /lib/i386-linux-gnu/
	[ -d /lib/tls/i686/cmov/ -a "$file" -nt /lib/tls/i686/cmov/$base ] && sudo cp -av "$file" /lib/tls/i686/cmov/

done
sudo mkdir -p /tmp/play3abn/
sudo chmod 770 /tmp/play3abn/
sudo chown -R $USER.$USER /tmp/play3abn/
perms &
PID=$!
echo "BGPID=$PID"
trap control_c SIGINT
[ "$1" != "skip" ] && sudo ./play3abn
sudo killall src-3abn lrucache
d0=$(stat -c "%d" /); d1=$(stat -c "%d" /sdcard); d2=$(stat -c "%d" /udisk)
[ "$d0" != "$d1" ] && sudo umount /sdcard
[ "$d0" != "$d2" ] && sudo umount /udisk
while [ true ]; do mcount=$(mount | grep /RadioSD | wc -l); [ "$mcount" -gt 1 ] && { sudo umount /media/RadioSD; continue; }; break; done
true
