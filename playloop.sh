#!/bin/sh

DIR="/tmp/play3abn"
T="$DIR/tmp"
Q="$T/playq"
P="$DIR/play.fifo"
STOP="$T/.stop.flag"

xdir=$(dirname "$0")
prevFill=1

# while [ true ]; do ogg123 -d raw -f - fill.ogg; done | aplay -f S16_LE -c1 -r16000
# For filler: ProgramList2-FILLER_ADS.txt     ProgramList2-FILLER_MUSIC.txt   ProgramList2-FILLER_OTHER.txt   ProgramList2-FILLER_SHORT.txt   ProgramList2-FILLER_SILENT.txt
# sort -R /sdcard/schedules/programs/ProgramList2-FILLER_MUSIC.txt

decode() {
	local file=$1
	local seek=$2
	local len=$3
	local msg=$4

	# WAS e.g. "/tmp/play3abn/tmp/playtmp-2011-11-21 11:10:28 0108 106-HEAVEN'S REALLY GONNA SHINE-Chuck Wagon Gang.ogg"
	# NOW e.g. "/tmp/play3abn/tmp/playtmp-1398709187 FILLER-I SEE GOD-King's Hearlds & Del Delker.ogg"
	base=$(basename "$file")
	local now=$(date +%s)
	printf "/tmp/play3abn/tmp/playtmp-%s %s%s" "$now" "$msg" "$base" >/tmp/play3abn/vars/playfile.txt
	printf "%d" "$len" >/tmp/play3abn/vars/curplaylen.txt

	[ "$seek" -lt "5" ] && seek=0

	date +"%D %T: DECODE seek=$seek; file=$file"
	local endtime=$((now+len-seek))
	local oldogg=$(ps waxf | grep ogg123 | grep -v grep | awk '{print $1}')
	local count=$(echo "$oldogg" | wc -l)
	[ "$count" -gt "1" ] && killall -9 ogg123 # Avoid overloading with extra processes (shouldn't happen but may anyway)
	ogg123 --skip "$seek" -d raw -f "$P" "$file" </dev/null >/tmp/play3abn/tmp/logs/ogg123.log 2>&1 &

	local start=$now
	local oplsec=0
	while [ $now -lt $endtime ]; do
		local playsec=$((now-start+seek))
		[ "$playsec" -gt 5 -a "$oldogg" != "" ] && { kill -9 $oldogg; oldogg=""; }
		plsec=$(cat /tmp/play3abn/vars/playedsec.txt)
		psdiff=$((plsec-oplsec))
		if [ "$psdiff" = "0" ]; then
			echo "0" >/tmp/play3abn/vars/playsec.txt # Paused at present
		else
			echo "$playsec" >/tmp/play3abn/vars/playsec.txt
		fi
		sleep 1
		oplsec=$plsec
		now=$(date +%s)
	done		
}

loadFill() {
	local fillType=$1
	[ "$fillType" = "" ] && echo "Missing fillType" 1>&2 && return
	files=$(find /tmp/play3abn/cache/schedules*/programs -name "ProgramList2-FILLER_$fillType.txt" -exec grep -v "^PATT" {} \; | sort -R)
#echo "fillType $fillType files=$files" 1>&2
	local fillCount=$(echo "$files" | wc -l)
	echo "fillType $fillType count=$fillCount" 1>&2
	eval fill$fillType=\$files
}

loadFills() {
	local ft
	for ft in MUSIC OTHERLONG OTHER ADS SHORT SILENT; do
		loadFill $ft
	done
}

getFill() {
	local ft="ADS";
	var="fill$ft"
  eval ret=$(echo \"\$$var\")
}

# DECIDE WHICH SET OF FILLER TO USE, and randomly get something that matches given length (or less)
getFiller() { # OUTPUT: len,tgt
	local maxLen=$1
	tgt=""
	len=0
	for x in 1 2 3 4 5; do
		[ "$maxLen" = "" ] && echo "Missing maxLen" 1>&2 && return
		# >=370     use MUSIC
		# 93-369    use OTHERLONG
		# 80-92     use OTHER
		# 61-79     use ADS
		# 10-60     alternate between SHORT and ADS
		# <=10      use SILENT
		local fillType="MUSIC" # Default
		if [ $maxLen -lt 370 ]; then
			[ $maxLen -ge 93 -a $maxLen -le 369 ] && fillType="OTHERLONG"
			[ $maxLen -ge 80 -a $maxLen -le 92  ] && fillType="OTHER"
			[ $maxLen -ge 61 -a $maxLen -le 79  ] && fillType="ADS"
			[ $maxLen -ge 10 -a $maxLen -le 60  ] && {
									prevFill=$((1-prevFill)); # Toggle fill type for this length
									if [ $prevFill -eq 1 ]; then fillType="SHORT"; else fillType="ADS"; fi
					}
			[ $maxLen -lt 10 ] && fillType="SILENT"
		fi
	#	echo $fillType
		maxLen=$(printf "%04d" "$maxLen")
		local fill=""
		local ret=""
		for t in 1 2; do
			local var="fill$fillType"
			eval ret=$(echo \"\$$var\")
			[ "$maxLen" -lt "1" ] && maxLen=1
			maxLen=$(echo "$maxLen" | bc) # Strip leading zeros misinterpreted as octal
			maxLen=$(printf "%04d" "$maxLen")
			fill=$(echo "$ret" | awk -v maxLen=$maxLen '{n=substr($0,1,4); if(n=="    ") { n="0000"; } if(n<=maxLen) { print n" "substr($0,6); } }' | head -n 1)
			[ "$fill" != "" ] && break
			loadFill "$fillType" # If nothing found, reload and retry
		done
		echo "  getFiller: fillType=$fillType,fill=$fill,maxLen=$maxLen" 1>&2
		if [ "$fill" = "" ]; then
			echo "BAD SCHEDULE FOR $fillType" 1>&2
			return
		fi
		local rest=$(echo "$ret" | grep -v "^$fill")
		eval fill$fillType=\$rest
		findFill "$fill"
		[ "$tgt" != "" ] && return
	done
	echo "getFiller: Unable to locate filler in 5 tries." 1>&2
}

findFill() { # OUTPUT: len,tgt
	local fill=$1
	echo "    findFill $fill" 1>&2
	len=${fill:0:4}
	len=$(echo "$len" | bc)
	fill=${fill:5}
	tgt=$(find /tmp/play3abn/cache/*/ -type f | grep "$fill" | head -n 1)
}

decodeLoop() {
	echo "decodeLoop: Start"
	while [ ! -f "$STOP" ]; do
		# Read links of format e.g.  "/tmp/play3abn/tmp/playq/2014-04-03 18:00:00 -1"
		before=$(date +%s)
# FIXME: below find needs to be in same process as read so filler assignments will be permanent
		links=$(find "$Q" -type l | sort)
		while IFS='\n' read name; do # Loop over links
			[ -f "$STOP" ] && break
			d=$(echo "$name" | sed -e 's@.*playq/@@g' | awk '{print $1" "$2}')
			at=$(date -d "$d" +%s)
			now=$(date +%s)
			# Convert from format e.g.: TESTIMONY4 3480 /tmp/play3abn/cache/download/TDYHR2~2011-05-06~3ABN_TODAY-LIVE_SIMULCAST_WITH_3ABN_TV~.ogg
			# or catcode=MELODY;expectSecs=0721;flag=366;url=/tmp/play3abn/cache/download/m/MFMH243B~MELODY_FROM_MY_HEART~~.ogg
			link=$(stat -c "%N" "$name" | sed -e 's/.*`//g' -e "s/'.*//g")
			cat=$(echo "$link" | sed -e 's/.*catcode=//g' -e 's/;.*//g')		# e.g. MELODY
			len=$(echo "$link" | sed -e 's/.*expectSecs=//g' -e 's/;.*//g')		# e.g. 0721
			flag=$(echo "$link" | sed -e 's/.*flag=//g' -e 's/;.*//g')		# e.g. 366
			tgt=$(echo "$link" | sed -e 's/.*url=//g' -e 's/;.*//g')
			if [ "${#len}" -gt "4" ]; then
				cat=$(echo "$link" | cut -d ' ' -f 1)
				len=$(echo "$link" | cut -d ' ' -f 2 | bc)
				tgt=$(echo "$link" | cut -d ' ' -f 3-)
			else
				len=$(echo "$len" | bc)
			fi
			echo "decodeLoop: name=$name,cat=$cat,len=$len,tgt=$tgt"
			
			nearend=$((at+len-5))
			if [ "$now" -lt "$at" ]; then
				flen=$((at-now))
				getFiller "$flen" # Output len,tgt
				if [ "$tgt" != "" ]; then
					echo "  decode fill $tgt" 1>&2
					decode "$tgt" 0 "$len" "FILLER-"
				else
					echo "  NO fill found: sleeping $flen" 1>&2
					sleep $flen
				fi
				break # Retry playing early file or get more filler
			fi
			now=$(date +%s)
			if [ "$now" -ge "$at" ]; then # This file should play or have been played
				if [ "$now" -lt "$nearend" ]; then # Play it
					seek=$((now-at))
					echo "decode prog $seek $tgt" 1>&2
					decode "$tgt" "$seek" "$len" ""
					rm "$name"
					break # Don't simply play next link; break to top level and see what links are not too late to play
				else
					echo "Too late for $name"
					rm "$name"
				fi
			fi
		done <<< "$links"
		after=$(date +%s)
		diff=$((after-before))
		[ "$diff" -lt 2 ] && sleep 1
	done
	echo "decodeLoop: Stop"
}

# Infinitely read and play audio data piped 
playLoop() {
	echo "playLoop: Start"
	while [ ! -f "$STOP" ]; do
		"./infinitepipe" "$P" | aplay -f S16_LE -c1 -r16000
	done
	echo "playLoop: Stop"
}

watchdog() {
	echo "watchdog: Start"
	while [ ! -f "$STOP" ]; do
		sleep 1
	done
	echo "watchdog: Stop"
	sleep 1
}

shutdown() {
	killall -9 infinitepipe aplay ogg123 >/dev/null 2>&1
}

main() {
	mkdir -p "$Q"
	mkdir -p "$DIR"
	rm -f "$P"
	mkfifo "$P"
	shutdown >/dev/null 2>&1
	loadFills
	# Begin main loops
	playLoop &
	decodeLoop &
	watchdog
	shutdown >/dev/null 2>&1
}

main


