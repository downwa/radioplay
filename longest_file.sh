#!/bin/sh

[ "$1" = "" ] && echo "Missing path to scan for longest file" && exit 1

for file in "$1"/*; do
	sec=$(mplayer -ao null -ss 9999 "$file" 2>/dev/null | grep " of .*(.*)" | awk '{print $5}')
	printf "%5.1f %s\n" "$sec" "$file"
done | tee /tmp/file_lens.txt
echo ""
echo "LONGEST FILE IS: " $(cat /tmp/file_lens.txt | sort -nr | head -n 1)
