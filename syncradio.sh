#!/bin/sh

SRC="UPDATE3ABN" # or RADIO3ABN, but that gets taken over for automatic playback

maincycle() {
printf "\r%s AWAITING DISK..." $(date +"%D %H:%M:%S")
############ AWAIT INSERTION OF USB DISK #################
while [ true ]; do
	grep -q sd[a-z][1-9]*$ /proc/partitions && break
	sleep 1
done
##### ACKNOWLEDGE INSERTION VIA LEDS #####
for note in 1 2 3 4 5; do
        echo 1 >/sys/class/leds/led0/brightness; sleep 1
        echo 0 >/sys/class/leds/led0/brightness; sleep 1
done 2>/dev/null

## NORMAL LED ##
echo mmc0 >/sys/class/leds/led0/trigger 2>/dev/null
##########################################################

printf "\r%s SYNCHRONIZING..." $(date +"%D %H:%M:%S")
tgtdir=$(mount | grep /dev/mmcblk0.*/sdcard.*vfat | awk '{print $3}' | while read tgtdir; do [ -d "$tgtdir/schedules" ] && echo "$tgtdir" && break; done)

if [ "$tgtdir" != "" ]; then
for dev in /dev/sd*; do mkdir -p "/media/$dev"; mount "$dev" "/media/$dev" && echo "Mounted $dev"; done
mount | grep /media | awk '{print $3}' | while read srcdir; do
	if [ -d "$srcdir/$SRC" ]; then
		printf "\rUpdating $tgtdir from $srcdir...\r"
		#rsync -auv "$srcdir/$SRC/download"/* "$tgtdir/download" | while read line; do printf "\r%s    \r" "$line"; done
		# rsync --itemize-changes -v --verbose -rlptgoDuv "$srcdir/$SRC/download"/* "$tgtdir/download" | while read line; do printf "\r%s    \r" "$line"; echo "$line" >/tmp/play3abn/vars/syncradio.txt; done
		rsync -rlptDuv --size-only "$srcdir/$SRC/download"/* "$tgtdir/download" | while read line; do printf "\r%s    \r" "$line"; echo "$line" >/tmp/play3abn/vars/syncradio.txt; done
		for dir in schedules filler Radio; do
			if [ -d "$srcdir/$SRC/$dir" ]; then
				rsync -rlptDuv --delete --size-only "$srcdir/$SRC/$dir"/* "$tgtdir/$dir" | while read line; do printf "\r%s    \r" "$line"; echo "$line" >/tmp/play3abn/vars/syncradio.txt; done
			fi
		done
	fi
done
sleep 1
for dev in /dev/sda*; do
#	fuser -mk "/media/$dev"
	umount -l "$dev"
	rmdir "/media/$dev" /media/dev
done 2>/dev/null
printf "\r                                                                                                      \r"
echo "SYNCHRONIZING COMPLETE.  REMOVE DISK."
else
echo "NO TARGET FOUND."
fi

########### NOTIFY COMPLETION ##############
## FLASH LED ##
while [ true ]; do
	grep -q sd[a-z][1-9]*$ /proc/partitions || break
	echo 1 >/sys/class/leds/led0/brightness; sleep 1
	echo 0 >/sys/class/leds/led0/brightness; sleep 1
done 2>/dev/null
## NORMAL LED ##
echo mmc0 >/sys/class/leds/led0/trigger 2>/dev/null
###########################################
}

mkdir -p /tmp/play3abn/vars
while [ true ]; do
	maincycle | while read line; do
		echo "$line" >/tmp/play3abn/vars/syncradio.txt
		echo "$line"
	done >/tmp/syncradio.out 2>/tmp/syncradio.err
	mv -f /tmp/syncradio.out /tmp/syncradio.out.old
	mv -f /tmp/syncradio.err /tmp/syncradio.err.old
done
