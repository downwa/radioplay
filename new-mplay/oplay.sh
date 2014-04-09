#!/bin/sh

Q="/tmp/play3abn/tmp/playq"
P="/tmp/play3abn/play.fifo"

prevFill=1

# while [ true ]; do ogg123 -d raw -f - fill.ogg; done | aplay -f S16_LE -c1 -r16000
# For filler: ProgramList2-FILLER_ADS.txt     ProgramList2-FILLER_MUSIC.txt   ProgramList2-FILLER_OTHER.txt   ProgramList2-FILLER_SHORT.txt   ProgramList2-FILLER_SILENT.txt
# sort -R /sdcard/schedules/programs/ProgramList2-FILLER_MUSIC.txt

decode() {
	local file=$1
	local seek=$2
	date +"%D %T: DECODE seek=$seek; file=$file"
	ogg123 --skip "$seek" -d raw -f "$P" "$file"
}

loadFill() {
	local fillType=$1
	[ "$fillType" = "" ] && echo "Missing fillType" 1>&2 && return
	eval fill$fillType=\$\(grep -v \"^PATT\" \"/RadioSD/schedules/programs/ProgramList2-FILLER_$fillType.txt\" \| sort -R\)
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
getFiller() {
	local maxLen=$1
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
	for t in 1 2; do
		local var="fill$fillType"
		eval ret=$(echo \"\$$var\")
		fill=$(echo "$ret" | awk -v maxLen=$maxLen '{n=substr($0,1,4); if(n=="    ") { n="0000"; } if(n<maxLen) { print n" "substr($0,6); } }' | head -n 1)
		[ "$fill" != "" ] && break
		loadFill "$fillType"
	done
	if [ "$fill" = "" ]; then
		echo "BAD SCHEDULE FOR $fillType" 1>&2
		return
	fi
	echo "$fill"
	#local rest=$(echo "$ret" | tail -n +2)
	local rest=$(echo "$ret" | grep -v "^$fill")
  eval fill$fillType=\$rest
}

main() {
				# Read links of format e.g.  "/tmp/play3abn/tmp/playq/2014-04-03 18:00:00 -1"
				find "$Q" -type l | sort | while read name; do
					d=$(echo "$name" | sed -e 's@.*playq/@@g' | awk '{print $1" "$2}')
					at=$(date -d "$d" +%s)
					now=$(date +%s)
					# Convert to format e.g.: TESTIMONY4 3480 /tmp/play3abn/cache/download/TDYHR2~2011-05-06~3ABN_TODAY-LIVE_SIMULCAST_WITH_3ABN_TV~.ogg
					link=$(stat -c "%N" "$name" | sed -e 's/.*`//g' -e "s/'.*//g")
					cat=$(echo "$link" | cut -d ' ' -f 1)
					len=$(echo "$link" | cut -d ' ' -f 2 | bc)
					tgt=$(echo "$link" | cut -d ' ' -f 3-)

					echo "name=$name,cat=$cat,len=$len,tgt=$tgt"
					
					end=$((at+len))
					if [ "$now" -lt "$at" ]; then
						flen=$((at-now))
						fill=$(getFiller "$flen")
						echo "Need filler $flen long before $name"
						sleep $flen 
					fi
					now=$(date +%s)
					if [ "$now" -ge "$at" ]; then # This file should play or have been played
						if [ "$now" -lt "$end" ]; then # Play it
							seek=$((now-at))
							decode "$tgt" "$seek"
						else
							echo "Too late for $name"
							rm "$name"
						fi
					fi
				done
}

main
