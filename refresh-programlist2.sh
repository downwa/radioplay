#!/bin/sh
# Keep ProgramList2 files up to date

progdir=$(dirname "$0")
gen="$progdir/programlist2-gen.sh"
which realpath >/dev/null || { echo missing realpath; exit 1; }
gen=$(realpath "$gen")

refresh() {
	find /tmp/play3abn/cache/schedules*/programs/Program-*.txt | while read file; do
		dir=$(dirname "$file")
		base=$(basename "$file" .txt | cut -d '-' -f 2-)
		printf "Refreshing $base...                           \r"
		pl="$dir/ProgramList2-$base.txt"
		same=$(stat -c "%Y" "$file" "$pl" | uniq | wc -l)
		if [ "$same" != "1" ]; then
			chvt 3 # Only display progress if anything had to be updated
			"$gen" "$base"
		fi
	done
}

main() {
date +"%D %T: Refreshing program lists..."
refresh
date +"%D %T: Done refreshing program lists."
chvt 1
}

# Wait until program lists are available (after play3abn starts)
while [ true ]; do
	count=$(find /tmp/play3abn/cache/schedules*/programs/Program-*.txt 2>/dev/null | wc -l)
	[ "$count" -gt 0 ] && break
	sleep 5
done

main >/dev/tty3
