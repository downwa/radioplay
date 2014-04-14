#!/bin/sh

[ "$1" = "" ] && echo "Specify category or --all" 1>&2 && exit 1

dir=$(dirname "$0")

find /tmp/play3abn/cache/schedules*/programs -type d | while read prgdir; do
	#echo "PRGDIR: $prgdir"
	find "$prgdir" -name 'Program-*.txt' | while read prgpath; do
		plname=$(echo "$prgpath" | sed -e 's@.*/Program-@@g')
		catcode=$(echo "$plname" | sed -e 's@.txt$@@g')
		[ "$1" != "--all" -a "$catcode" != "$1" ] && continue
		#echo "CATCODE: $catcode"
		plpath="$prgdir/ProgramList2-$plname"
		printf "\rPLPATH: %50s     \r" "$plpath"
		cat "$prgpath" | cut -d '#' -f 1 | sed -e 's/ *$//g' | grep -v "^$" | while read patt; do
			patt=$(echo "$patt" | sed -e 's/(/\\(/g' -e 's/)/\\)/g')
			items=$(find /tmp/play3abn/cache/download*/ -name '*.ogg' | grep "/$patt")
			#echo "/tmp/play3abn/cache/download* PATT=$patt;ITEMS=$items." 1>&2
			if [ "$items" = "" ]; then
				patt=$(echo "$patt" | sed -e 's@../Radio/@@g')
				items=$(find /tmp/play3abn/cache/Radio*/ -name '*.ogg' | grep "/$patt")
			#echo "/tmp/play3abn/cache/Radio* PATT=$patt;ITEMS2=$items." 1>&2
			fi
			if [ "$items" = "" ]; then
				patt=$(echo "$patt" | sed -e 's@../filler/@@g')
				items=$(find /tmp/play3abn/cache/filler*/ -name '*.ogg' | grep "/$patt")
			fi
			if [ "$items" != "" ]; then
				items=$(echo "$items" | while read path; do
					file=$(echo "$path" | sed -e 's@/tmp/play3abn/cache/[a-zA-Z0-9]*/@@g')
					grep "$file" "$plpath" 2>/dev/null || { 
						#seclen=$("$dir/ogglen" "$path" 2>/dev/null | cut -d ' ' -f 1)
						seclen=$(ogginfo "$path" | grep "^.*Playback length: " | cut -d ' ' -f 3- | sed -e 's/^/(/g' -e 's/m:/*60+/g' -e 's@s@+0.5)/1@g' | bc)
						[ "$seclen" = "" ] && seclen="0"
						printf "%04d %s\n" "$seclen" "$file"
						printf "\n%04d %s\r" "$seclen" "$file" 1>&2
					}
				done)
				if [ "$items" != "" ]; then
					echo "PATT: $patt"
					echo "$items" | sort -k 2 | uniq
				fi
			fi			
		done >"$plpath.tmp" && mv "$plpath.tmp" "$plpath"
		touch "$prgpath" "$plpath" # Synchronize change time
	done
done
