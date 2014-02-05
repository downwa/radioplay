#!/bin/sh

# Record files we cannot take md5sum for
>md5sums.txt
for typ in b c d p l s; do
	find -type $typ -exec echo "00000000000000000000000000000000  "{} \; >>md5sums.txt
done
find -type f | while read file; do md5sum "$file"; done >>md5sums.txt
