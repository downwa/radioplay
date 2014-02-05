#!/bin/sh

pidof dopatch.sh | grep " " >/dev/null && echo "Already updating..." && exit 1
verbose=1
if [ "$(whoami)" = "root" -a "$SHELL" = "/bin/sh" ]; then # Likely running from QTopia
	verbose=0
fi

latest="latest.htm"
doalpha=$(cat /play3abn/vars/alpha.txt 2>/dev/null)
[ "$doalpha" = "" ] && doalpha=$(cat /recorder/vars/alpha.txt 2>/dev/null)
if [ "$doalpha" = "1" ]; then
	latest="alpha.htm"
	rm -f /play3abn/vars/alpha.txt /recorder/vars/alpha.txt # Only stay in Alpha mode through one update!
fi

dopatch() {
	local file
	latest=$2
	if [ "$1" = "" ]; then # Find latest patch
		#latest=$(find ../patches/ 2>/dev/null | sort | tail -n 1)
		hostip=""
		rm -f "$latest"
		[ "$(whoami)" = "warren" ] && hostip="127.0.0.1"
		if [ "$hostip" = "" ]; then
			netstat -n 2>/dev/null | grep "^tcp.*:23 " | awk '{print $4}' | grep -q :23 && netstat -n 2>/dev/null | grep "^tcp.*:23 " | awk '{print $5}' | cut -d ':' -f 1 | sort | uniq | while read ip; do
				baseurl="http://$ip/3abnrelease"
				echo wget "$baseurl/$latest"
				wget "$baseurl/$latest" && { echo "$ip" >/tmp/hostip.txt; break; }
			done
		fi
		[ "$hostip" = "" ] && hostip=$(cat /tmp/hostip.txt)
		if [ "$hostip" = "" ]; then
			baseurl="http://iglooware.com/3abnrelease"
			echo wget "$baseurl/$latest"
			wget "$baseurl/$latest"
		else
			baseurl="http://$hostip/3abnrelease"
		fi
		file=$(cat $latest | grep 3abn- | cut -d '"' -f 2 | grep .arc3)
		[ "$file" = "" ] && echo "Unable to check status." && return
		xarchive=$(which xarchive)
		#curlpart=$(which curlpart)
		if [ "$xarchive" = "" ]; then #-o "$curlpart" = "" ]; then
			echo wget "$baseurl/xarchive"
			wget "$baseurl/xarchive"
			#echo wget "$baseurl/curlpart"
			#wget "$baseurl/curlpart"
			chmod +x xarchive # curlpart
			mv xarchive /bin/
			xarchive=$(which xarchive)
			curlpart=$(which curlpart)
			if [ "$curlpart" = "" ]; then # May be an old version without proper libs
				echo wget "$baseurl/libs.tar.bz2"
				wget "$baseurl/libs.tar.bz2"
				bunzip2 libs.tar.bz2
				tar xvf libs.tar
				rm -f libs.tar
			fi
    fi
		free=$(df -k /sdcard | grep /dev/ | awk '{print $4}')
		if [ "$free" = "" ]; then # Not mounted
			mknod /dev/mmcblk1 b 179 1 2>/dev/null
			mount -t vfat -o codepage=437 /dev/mmcblk1 /sdcard
			free=$(df -k /sdcard | grep /dev/ | awk '{print $4}')
		fi
		if [ "$free" != "" -a "true" = "false" ]; then # It's mounted
			echo "Renaming filler..."
			for file in Filler-*.ogg; do new=${file:7:3}; mv $file songs/$new-Filler.ogg; done
			echo "Downloading filler..."
			#rm -fr /filler # Filler now on sd card
			mkdir -p /sdcard/filler/songs
			cd /sdcard
			rm -f filler.txt
			wget "$baseurl/sdcard/filler.txt"
			cat filler.txt | while read line; do
				echo "Fetching $baseurl/sdcard/$line"
				if [ "$free" -lt 8000 ]; then
					echo "Freeing 8 more Mb for filler..."
					for day in $(seq 60 -5 10); do
						if [ "$free" -lt 8000 ]; then # Need to free space for the new filler material
							echo "Removing files from $day to free space (free=$free)"
							echo find /sdcard/download -type f -mtime +$day -delete -print
							find /sdcard/download -type f -mtime +$day -delete -print
							free=$(df -k /sdcard | grep /dev/ | awk '{print $4}')
						else
							break
						fi
					done
				fi
				echo "wget $baseurl/sdcard/$line"
				wget -c -O "$line" "$baseurl/sdcard/$line" || continue #& # Resume in case last attempt timed out
				pid=$!
				size=0
				sleeptime=10
# 				echo "Watching pid=$pid"
# 				while [ true ]; do
# 					sleep $sleeptime
# 					newsize=$(ls -l "$line" | awk '{print $5}')
# 					[ "$newsize" = "$size" ] && kill -9 "$pid" && break # No progress in last period
# 					sleeptime=5
# 				done
# 				echo "Ready for next file..."
			done
			cd /
		fi
	else # Patch from specified file
		file=$1
	fi

	# 08:90:90:90:90:90 209.165.180.164 209.165.180.164 UNITED STATES ALASKA ANCHORA
	# NOTE: Here try to get information for a specific client and send it in for debugging, if needed
	######################
	dir=$(dirname "$file")
	oldrel=$(cat play3abn/vars/release.txt 2>/dev/null)
	[ "$oldrel" = "" ] && oldrel=$(cat recorder/vars/release.txt 2>/dev/null)
	[ "$oldrel" = "" ] && oldrel=0
	rel=$(echo ${file} | sed -e 's/.*3abn-//g' -e 's/.zip$//g' -e 's/.bin$//g' -e 's/.arc3$//g')
	echo "oldrel=$oldrel,rel=$rel."
	if [ "$oldrel" -lt "$rel" ]; then
		touch /tmp/appliedpatch.flag
		[ "$verbose" = "1" ] && echo "Applying patch: $file"
# 		/play3abn/play3abn.arm --resetscr 2>/dev/null
# 		clear >/dev/tty1
# 		printf "\n\n\n\n\n\n\n\n\n" >/dev/tty1
# 		echo "   Updating: $rel" >/dev/tty1
		if [ ! -f "$file" ]; then
			if [ "$baseurl" != "" ]; then
				xarchive "$baseurl/$file" xarchive
			else
				echo "File not found: $file"
			fi
		else
			xarchive "$file" xarchive
		fi
		for exe in bgprocs.sh play.sh; do
			strings /tmp/xarchive.log.txt | grep -q "$exe: Existing" || echo "$exe" >> /tmp/doreboot.txt
		done
		for exe in play3abn.arm; do
			strings /tmp/xarchive.log.txt | grep -q "$exe: Existing" || echo "$exe" >> /tmp/restartexe.txt
		done
	fi
}

> /tmp/restartexe.txt
> /tmp/doreboot.txt
if [ "$(whoami)" = "root" ]; then # Only need relative paths for testing on non-root account
	cd /
fi
cwd=$(pwd)
# Check for installable archive
mount /sdcard -o remount,async 2>/dev/null # Speed up writes to SD card (if any)

# Make room to process archives
if [ "$(whoami)" = "root" ]; then
	if [ -f /root/Documents/New*Soul.mp3 ]; then
		#touch /tmp/cleanup.flag
		for file in songti_* unifont_* helvetica_*; do rm -f /opt/Qtopia/lib/fonts/$file; done # Clean up large fonts
		rm -fr /root/Documents/* /opt/* # Clean up initial mini2440 setup
	fi
fi
touch /tmp/upgrading.flag

find /sdcard/*.arc3 /udisk/*.arc3 -maxdepth 0 2>/dev/null | while read archive; do
	dopatch "$archive" "$latest"
done
dopatch "" "$latest"

grep -q play.sh /tmp/doreboot.txt && echo play3abn.arm >> /tmp/doreboot.txt # play.sh must restart play3abn.arm
for exe in $(cat /tmp/restartexe.txt | sort | uniq); do
	ee=$(basename "$exe")
	pidof "$ee" >/dev/null && killall -9 "$ee"
done

umount /sdcard 2>/dev/null
rm -f /sdcard/filler 2>/dev/null
ln -s /filler /sdcard/filler 2>/dev/null
mknod /dev/mmcblk1 b 179 1 2>/dev/null
opts="rw,async,nosuid,nodev,noatime,nodiratime,fmask=0000,dmask=0000,allow_utime=0022,iocharset=utf8"
mount -o $opts /dev/mmcblk1 /sdcard 2>/dev/null
if [ -d /recorder -a -d /play3abn -a -f /play3abn/play3abn.arm -a -f /tmp/appliedpatch.flag ]; then # Old version to remove
	rm -fr /recorder
	reboot
	sleep 5
	reboot -d 5 -f # In case init doesn't reboot
fi
rm -f /tmp/restartexe.txt
if [ -s /tmp/doreboot.txt ]; then
	echo rm -f /tmp/doreboot.txt
	clear >/dev/tty1
	rel=$(cat /play3abn/vars/release.txt)
	printf "\n\n\n\n\n\n\n\n\n" >/dev/tty1
	echo "   Complete: $rel" >/dev/tty1
	if [ "$verbose" = "0" ]; then # May be running from QTopia
		echo "   Rebooting..." >/dev/tty1
		reboot
		sleep 5
		reboot -d 5 -f # In case init doesn't reboot
	fi
fi
if [ -f /tmp/upgrading.flag ]; then
       rm -f /tmp/upgrading.flag
fi
rm -f /tmp/appliedpatch.flag

exit
EOF
