#!/bin/sh

radio=$(mount | grep -i radio | awk '{print $3}' | sort | tail -n 1) # Detect media
[ "$radio" = "" ] && echo "No RADIO media found." 1>&2 && exit 1

echo "Caching: $radio"
find "$radio/schedules/programs/" -name 'Program-*.txt' | while read program; do
	prgbase=$(basename "$program" .txt | cut -d '-' -f 2-)
	#[ "$program" != "/media/RadioSD/schedules/programs/Program-CREATION.txt" ] && continue
	echo "Caching $prgbase"
	cat "$program" | sed -e 's/#.*//g' -e 's/ *$//g' |  while read patt; do
		[ "$patt" = "" ] && continue
		echo "PATT: $patt"
		for loc in download Radio; do
			[ ! -d "$radio/$log" ] && continue
			cd "$radio/$loc"
			find . -type f -name '*.ogg' | sed -e 's@^./@@g' | grep "^$patt" | sort | while read name; do
				len=$(ogginfo "$name" | grep "Playback length: " | awk '{print $3}' | sed -e 's/m/*60/g' -e 's/:/+/g' -e 's/s/+0.5/g' -e 's/^/scale=0;(/g' -e 's@$@)/1@g' | bc)
				printf "%04d %s\n" "$len" "$loc/$name"
			done
		done
	done #>"$radio/schedules/programs/ProgramList2-$prgbase.txt"
done

