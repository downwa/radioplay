#!/bin/sh

SRC="UPDATE3ABN" # or RADIO3ABN, but that gets taken over for automatic playback

makepart() {
	ptype=$1
	type=$2
	start=$3
	end=$4
	fs=$5
	num=$6
	echo "Making partition $num"
	umount tgt/$num 2>&1 | egrep -vi "not found|not mounted"
	parted -s $device "rm $num" 2>&1 | grep -v "Partition doesn't exist."
echo parted -s $device "mkpart $ptype $type $start $end"
	parted -s $device "mkpart $ptype $type $start $end"
#       fdisk -l $device
	if [ "$type" != "" ]; then
					echo "Making filesystem $fs on $num"
					if [ "$fs" = "ext4" ]; then
									mkfs.$fs -m 0 ${device}p$num
					else
									mkfs.$fs ${device}p$num
					fi
					mkdir -p tgt/$num
					mount ${device}p$num tgt/$num
					sleep 1
					cp -av --no-preserve=ownership src/$num/* tgt/$num/
					umount tgt/$num
	fi
}

fixexpart() { # Turn this into an extended Linux partition
fdisk "$device" >/tmp/mksd.out <<EOF
t
2
85
w
EOF
}

mksd() {
	local device=$1

}

download() {				
	local srcdir=$1
	local tgtdir=$1
	printf "\rUpdating $tgtdir from $srcdir...\r"
	#rsync -auv "$srcdir/$SRC/download"/* "$tgtdir/download" | while read line; do printf "\r%s    \r" "$line"; done
	# rsync --itemize-changes -v --verbose -rlptgoDuv "$srcdir/$SRC/download"/* "$tgtdir/download" | while read line; do printf "\r%s    \r" "$line"; echo "$line" >/tmp/play3abn/vars/syncradio.txt; done
	rsync -rlptDuv --size-only "$srcdir/$SRC/download"/* "$tgtdir/download" | while read line; do printf "\r%s    \r" "$line"; echo "$line" >/tmp/play3abn/vars/syncradio.txt; done
	for dir in schedules filler Radio; do
		if [ -d "$srcdir/$SRC/$dir" ]; then
			rsync -rlptDuv --delete --size-only "$srcdir/$SRC/$dir"/* "$tgtdir/$dir" | while read line; do printf "\r%s    \r" "$line"; echo "$line" >/tmp/play3abn/vars/syncradio.txt; done
		fi
	done
}

checkDownloads() {
	for dev in /dev/sd*; do mkdir -p "/media/$dev"; mount "$dev" "/media/$dev" && echo "Mounted $dev"; done
	mount | grep /media | awk '{print $3}' | while read srcdir; do
		# DOWNLOAD to PI
		[ -d "$srcdir/$SRC" ] && download "$srcdir" "$tgtdir"
	done
}

upload() {
	local srcdir=$1
	local tgtdir=$1
	local free=$(df -P "$srcdir" | grep "$srcdir" | awk '{print $4}')
	local count=$(find "$srcdir" -type f | wc -l)
	if [ "$free" -gt "28000000" -a "$count" = "0" ]; then #Enough space to create
		mksd "$dev"
	fi
}

# RETURN how many files are used on all inserted devices (should be zero to be safe)
filecount() {
	filecount=$(echo $(for dev in /dev/sd*; do
		umount "$dev" 2>/dev/null
		d="/media/$dev"; mkdir -p "$d"
		mount "$dev" "$d" 2>/dev/null && {
			find "$d" -mindepth 1 | wc -l; true;
		} || { rmdir "$d"; }; done
	) | sed -e 's/ /+/g' | bc)
}

checkUploads() {
	filecount
	[ "$filecount" -gt "0" ] && return # Don't want to trash any disks
	for dev in /dev/sd*; do
		devfree=$(parted "$dev" print | grep "^Disk.*$dev" | awk '{print $3}' | sed -e 's@GB@*1024/1@g' | bc)
		[ "$devfree" -lt "30000" ] && continue # Not enough space on this device
		echo "$dev"
	done | sed -e 's/[0-9]$//g' | sort | uniq | while read dev; do # May have more than one device plugged in that is blank
		# UPLOAD from PI
		echo upload "$tgtdir" "$srcdir" # Reverse
		sleep 5
	done
}

doCleanup() {
	for dev in /dev/sda*; do
	#	fuser -mk "/media/$dev"
		umount -l "$dev"
		rmdir "/media/$dev" /media/dev
	done 2>/dev/null
}

notifyWaiting() {
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
}

notifyDone() {
	printf "\r                                                                                                      \r"
	echo "SYNCHRONIZING COMPLETE.  REMOVE DISK."

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

findTarget() {
	tgtdir=$(mount | grep /dev/mmcblk0.*/sdcard.*vfat | awk '{print $3}' | while read tgtdir; do
		[ -d "$tgtdir/schedules" ] && echo "$tgtdir" && break
	done)
}

maincycle() {
	findTarget
	if [ "$tgtdir" = "" ]; then
		echo "NO TARGET FOUND."
		return
	fi		
	notifyWaiting
	printf "\r%s SYNCHRONIZING..." $(date +"%D %H:%M:%S")
	checkDownloads		
	checkUploads
	sleep 1
	doCleanup	
	notifyDone
}

main() {
	sleep 30 # Wait for main system to start and mount dirs
	mkdir -p /tmp/play3abn/vars
	while [ true ]; do
		maincycle | while read line; do
			echo "$line" >/tmp/play3abn/vars/syncradio.txt
			echo "$line"
		done >/tmp/syncradio.out 2>/tmp/syncradio.err
		mv -f /tmp/syncradio.out /tmp/syncradio.out.old
		mv -f /tmp/syncradio.err /tmp/syncradio.err.old
	done
}
	
main