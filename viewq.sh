#!/bin/sh

find /tmp/play3abn/tmp/playq/ -type l | sort | while read file; do
	tgt=$(stat -c "%N" "$file" | cut -d '`' -f 3- | cut -d "'" -f 1 | cut -d ' ' -f 3-)
	stat -c "${file:24} *%n" "$tgt" 2>/dev/null || echo "${file:24}  $tgt"
done | less
