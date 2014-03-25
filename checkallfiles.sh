#!/bin/sh

# Check data files
cat allfiles.txt | grep "[0-9][0-9]* 1" | while read size path; do
	[ -d "/sdcard$path" ] && continue # Skip directories
	result=$(find "/sdcard$path" -type f -printf "%s %p\n" 2>/dev/null)
	if [ "$result" = "" ]; then # Not found
		base=$(basename "$path")
		dir=$(dirname "/sdcard$path")
		result=$(find "/sdcard1" -name "$base" -type f -printf "%s %p\n")
		if [ "$result" != "" ]; then
			echo "$result" | while read fsize fpath; do
				if [ "$fsize" = "$size" ]; then
					mkdir -p "$dir"
					mv -v "$fpath" "$dir"
					fdir=$fpath
					while [ true ]; do
						fdir=$(dirname "$fdir")
						rmdir -v "$fdir" 2>/dev/null || break
						[ "$fdir" = "/" ] && break
					done
				else
					echo "SIZE mismatch: $fsize != $size: $fpath"
				fi
			done
		else
			echo "Missing $path"
		fi
	else
		echo "$result" | while read fsize fpath; do [ "$fsize" != "$size" ] && echo "SIZE mismatch: $fsize != $size: $path"; done
	fi
done
