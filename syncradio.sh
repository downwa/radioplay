#!/bin/sh

SRC="UPDATE3ABN" # or RADIO3ABN, but that gets taken over for automatic playback

makepart() {
	ptype=$1
	type=$2
	start=$3
	end=$4
	fs=$5
	num=$6	
	src=$7
	
	echo "Making partition $num"
	umount tgt/$num 2>&1 | egrep -vi "not found|not mounted"
	parted -s $device "rm $num" 2>&1 | grep -v "Partition doesn't exist."
echo parted -s $device "mkpart $ptype $type $start $end"
	parted -s $device "mkpart $ptype $type $start $end"
#       fdisk -l $device
	if [ "$type" != "" ]; then
					echo "Making filesystem $fs on $num"
					if [ "$fs" = "ext4" ]; then
									mkfs.$fs -m 0 ${device}$num
					else
									mkfs.$fs ${device}$num
					fi
					mkdir -p /mnt/tgt/$num
					mount ${device}$num /mnt/tgt/$num 2>&1 || echo "Mount ${device}$num failed."
					sleep 1
					cp -av --one-file-system --no-preserve=ownership "$src"/. /mnt/tgt/$num/
					umount /mnt/tgt/$num
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
	umount /dev/sd* 2>&1 | egrep -vi "not found|not mounted"
	dd if=/dev/zero of="$device" bs=512 count=1 2>/tmp/mksd.err
	parted -s $device "mklabel msdos"
	srcs=$(mount | egrep "^/dev/root|^/dev/mmcblk" | sort | awk '{print $3}' | grep -v ^/var/log)
	src1=$(echo "$srcs" | head -n 1)
	src2=$(echo "$srcs" | head -n 2 | tail -n 1)
	src3=$(echo "$srcs" | tail -n 1)
	makepart primary  fat32 0%      29000M  vfat 1 "$src1"
	makepart extended ""    29000M  100%    ""   2
	fixexpart
	makepart logical  fat32 29001M  29050M  vfat 5 "$src2"
	makepart logical  ext2  29051M  100%    ext4 6 "$src3"
}

download() {				
	local srcdir=$1
	local tgtdir=$1
	printf "Updating $tgtdir from $srcdir...\n"
	#rsync -auv "$srcdir/$SRC/download"/* "$tgtdir/download" | while read line; do printf "%s\n" "$line"; done
	# rsync --itemize-changes -v --verbose -rlptgoDuv "$srcdir/$SRC/download"/* "$tgtdir/download" | while read line; do printf "%s\n" "$line"; echo "$line" >/tmp/play3abn/vars/syncradio.txt; done
	rsync -rlptDuv --size-only "$srcdir/$SRC/download"/* "$tgtdir/download" | while read line; do printf "%s\n" "$line"; echo "$line" >/tmp/play3abn/vars/syncradio.txt; done
	for dir in schedules filler Radio; do
		if [ -d "$srcdir/$SRC/$dir" ]; then
			rsync -rlptDuv --delete --size-only "$srcdir/$SRC/$dir"/* "$tgtdir/$dir" | while read line; do printf "%s\n" "$line"; echo "$line" >/tmp/play3abn/vars/syncradio.txt; done
		fi
	done
}

checkDownloads() {
	echo "checkDownloads from $SRC"
	for dev in /dev/sd*; do mkdir -p "/media/$dev"; mount "$dev" "/media/$dev" 2>/dev/null && echo "Mounted $dev"; done
	mount | grep /media | awk '{print $3}' | while read srcdir; do
		# DOWNLOAD to PI
		[ -d "$srcdir/$SRC" ] && download "$srcdir" "$tgtdir"
	done
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
	echo "checkUploads"
	filecount
	echo "filecount=$filecount"
	if [ "$filecount" -gt "0" ]; then
		srcs=$(mount | egrep "^/dev/root|^/dev/mmcblk" | sort | awk '{print $3}' | grep -v ^/var/log)
		src1=$(echo "$srcs" | head -n 1)
		src2=$(echo "$srcs" | head -n 2 | tail -n 1)
		src3=$(echo "$srcs" | tail -n 1)
		echo "src1=$src1 (partition 1, VFAT, Radio Data+boot)"
		echo "src2=$src2 (partition 5, VFAT, boot)"
		echo "src3=$src3 (partition 6, ext4, root)"
		for dev in /dev/sd*1 /dev/sd*5 /dev/sd*6; do
			n=${dev:8}
			umount "$dev" 2>/dev/null
			d="/mnt/tgt/$n"; mkdir -p "$d"
			echo "CHECKING $dev..."
			mount "$dev" "$d" 2>/dev/null && {
				[ "${dev:8:1}" = "1" ] && cmp=$src1
				[ "${dev:8:1}" = "5" ] && cmp=$src2
				[ "${dev:8:1}" = "6" ] && cmp=$src3
				#agediff=$(echo $(stat -c "%Y" "$cmp/." "$d/.") | sed -e 's/ /-/g' | bc)
				agediff=1 # Always sync TO USB (not the reverse, at least not automatically)
				if [ "$agediff" -gt "0" ]; then
					echo "SYNC TO USB"
					sleep 5
					rsync --one-file-system -av --delete "$cmp/." "$d/."
				else
					if [ "$cmp" = "$src1" ]; then
						echo "SYNC DOWN FROM USB"
						sleep 5
						rsync --one-file-system -av "$d/." "$cmp/."
					else
						echo "NOT SYNCING ROOT"
						sleep 5
					fi
				fi
			}
			umount "$dev" 2>/dev/null
		done
		return # Don't want to trash any disks
	fi
	echo "CHECKING FREE SPACE TO MASTER TO USB..."
	for dev in /dev/sd*; do
		devfree=$(parted "$dev" print | grep "^Disk.*$dev" | awk '{print $3}' | sed -e 's@GB@*1024/1@g' | bc)
		[ "$devfree" -lt "30000" ] && continue # Not enough space on this device
		echo "$dev"
	done | sed -e 's/[0-9]$//g' | sort | uniq | while read dev; do # May have more than one device plugged in that is blank
		echo "MASTERING TO USB..."
		# UPLOAD from PI.  Will REPLACE all contents of $dev
		mksd "$dev"
		#sleep 5
	done
}

doCleanup() {
	for dev in /dev/sd*; do
	#	fuser -mk "/media/$dev"
		umount -l "$dev"
		rmdir "/media/$dev" /media/dev
	done 2>/dev/null
}

notifyWaiting() {
	date=$(date +"%D %H:%M:%S")
	printf "%s AWAITING DISK...\n" "$date"
	############ AWAIT INSERTION OF USB DISK #################
	while [ true ]; do
		grep -q sd[a-z][1-9]*$ /proc/partitions && break
		sleep 1
	done
	printf "%s DETECTED DISK.\n" "$date"
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
	#printf "\r                                                                                                      \r"
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
	echo "findTarget"
	tgtdir=$(mount | grep /dev/mmcblk0.*/sdcard.*vfat | awk '{print $3}' | while read tgtdir; do
		[ -d "$tgtdir/schedules" ] && echo "$tgtdir" && break
	done)
}

maincycle() {
	findTarget
	if [ "$tgtdir" = "" ]; then
		echo "NO TARGET FOUND."
		sleep 1
		return
	fi		
	notifyWaiting
	date=$(date +"%D %H:%M:%S")
	printf "%s SYNCHRONIZING...\n" "$date"
	checkDownloads		
	checkUploads
	sleep 1
	doCleanup	
	notifyDone
}

main() {
	>"/tmp/play3abn/vars/syncradio.txt"
	sleep 30 # Wait for main system to start and mount dirs.  OTHERWISE, sync WILL go awry!
	mkdir -p /tmp/play3abn/vars
	while [ true ]; do
		maincycle | while read line; do
			free=$(echo $(df -h /mnt/tgt/* | egrep -v "^Filesystem|^/dev/root" | sort | uniq | sed -e 's@^/dev/@@g' -e 's/%/%%/g' | awk '{print $1"-"$5}'))
			date=$(date +"%H:%M:%S")
			echo "$date $free $line" | strings >/tmp/play3abn/vars/syncradio.txt
			echo "$date $free $line" 1>&2
		done >/tmp/syncradio.out 2>/tmp/syncradio.err
		mv -f /tmp/syncradio.out /tmp/syncradio.out.old
		mv -f /tmp/syncradio.err /tmp/syncradio.err.old
	done
}
	
main
