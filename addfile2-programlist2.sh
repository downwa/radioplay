#!/bin/sh

newfile=$1
[ "$newfile" = "" ] && echo "Missing [newfile] to index." 1>&2 && exit 1

indexfile() {
	newfile=$1
	plpath=$2
	cat "$plpath"
	file=$(echo "$newfile" | sed -e 's@.*/@@g')
	grep -q "$file" "$plpath" && echo "Already have $file in $plpath" 1>&2 && return
        export LD_LIBRARY_PATH=/play3abn                                                                                                     
        seclen=$(./ogglen "$newfile" 2>/dev/null | cut -d ' ' -f 1)                                                                          
	#seclen=$(ogginfo "$newfile" | grep "^.*Playback length: " | cut -d ' ' -f 3- | sed -e 's/^/(/g' -e 's/m:/*60+/g' -e 's@s@+0.5)/1@g' | bc)
	printf "%04d %s\n" "$seclen" "download/$file"
}

findpattern() {
	newfile=$1
	find /tmp/play3abn/cache/schedules*/programs -type d | while read prgdir; do
		find "$prgdir" -name 'Program-*.txt' | while read prgpath; do
			plname=$(echo "$prgpath" | sed -e 's@.*/Program-@@g')
			plpath="$prgdir/ProgramList2-$plname"
			cat "$prgpath" | cut -d '#' -f 1 | sed -e 's/ *$//g' | grep -v "^$" | while read patt; do
				echo "$newfile" | grep -q -E "$patt" || continue
				echo "PATT: $patt"
				indexfile "$newfile" "$plpath" | sort -k 2 | uniq
			done >"$plpath.tmp"
			mv "$plpath.tmp" "$plpath"
		done
	done
}

findpattern "$newfile"
