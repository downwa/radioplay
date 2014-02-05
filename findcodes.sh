#!/bin/sh

BASEDIR="/tmp/play3abn/cache/"
cd "$BASEDIR"
oldout=""
find "." -type f | while read file; do
	[ "$file" = "./encodeloop.sh" -o "$file" = "./makeogg.sh" -o "$file" = "./out.wav" -o "$file" = "./out2.wav" ] && continue
	#dir=$(dirname "$subfile")
	dir=$(dirname "$file")
	origdir=$dir
	leveldown=0
	while [ true ]; do
		digits=$(basename "$dir" | cut -d '/' -f 3- | sed 's/[^0-9]*\([0-9]*\)[^0-9]*/\1/g') ## Keep only the digits of the last directory
		[ "$digits" = "" ] && break ## If directory has no digits, consider it top-level
		ndir=$(dirname "$dir")
		[ "$ndir" = "." ] && break ## If this would eradicate directory, it is top-level
		dir=$ndir
		leveldown=$((leveldown+1))
	done
	validcode=0
	base=$(basename "$dir")
	name=$(basename "$file")
	code=$(echo "$name" | sed -e 's/\([a-zA-Z][a-zA-Z]*\)[0-9][0-9]*.*/\1/g') # Strip trailing digits from name
	if [ "$code" = "$name" ]; then
		code=$(echo "$base" | sed -e 's/ //g' -e 's/\([a-zA-Z][a-zA-Z]*\)[0-9][0-9]*.*/\1/g')
	else
		validcode=1
	fi
	[ "${#code}" -lt 2 ] && code=$(echo "$base" | sed -e 's/ //g' -e 's/\([a-zA-Z][a-zA-Z]*\)[0-9][0-9]*.*/\1/g')
	music=$(echo "$file" | sed 's/.*\(Music\).*/\1/g') ## Contains "Music"
	if [ "$music" = "Music" ]; then
		code="Music"
		match=$(echo "$BASEDIR${origdir:2}" | sed 's/\(Music\).*/\1/g')
	else
		story=$(echo "$file" | sed 's/.*\(Story\).*/\1/g') ## Contains "Story"
		if [ "$story" = "Story" ]; then
			code="Story"
			match=$(echo "$BASEDIR${origdir:2}" | sed 's/\(Story\).*/\1/g')
		else
			#digits=$(echo "$code" | sed 's/[^0-9]*\([0-9]*\)[^0-9]*/\1/g') ## Only contains digits
			startdigits=$(echo "$name" | sed 's/^\([0-9]*\).*/\1/g') ## Starts with digits
			#echo "startdigits=$startdigits;code=$code;base=$base;name=$name;origdir=$origdir"
			if [ "$startdigits" != "" ]; then
				##code=$(basename "$origdir" | sed -e 's/ //g')
				code=$(echo "$base" | sed -e 's/ //g' -e 's/\([a-zA-Z][a-zA-Z]*\)[0-9][0-9]*.*/\1/g')
				if [ "$validcode" = "1" -a "$leveldown" = "0" ]; then
					match="$BASEDIR${dir:2}/$startdigits"
				else
					match="$BASEDIR${dir:2}/"
				fi
			else
				if [ "$validcode" = "1" -a "$leveldown" = "0" ]; then
					match="$BASEDIR${dir:2}/$code"
				else
					match="$BASEDIR${dir:2}/"
				fi
			fi
		fi
	fi
	code=$(echo "$code" | tr '[:lower:]' '[:upper:]') ## Uppercase the code
	#echo "$base;$code;match=$match*"
	out="$code;match=$match*"
	[ "$out" != "$oldout" ] && echo "$out"
	oldout=$out
done #| sort | uniq
