#!/bin/sh

OUT="$HOME/play3abn.run"
TGTDIR="$HOME/dev/3abn/fullroot/Linux_armv4tl"
tdir="$HOME/dev/3abn/partroot"

cd "$TGTDIR"
find . >allfiles.txt
#./limitedfiles.sh ## FIXME: DECIDE if flite is useful to keep or not.
mkdir -p "$tdir"
cat allfiles.txt | egrep -v "flite|*~$" | while read file; do
	if [ "$file" != "./etc/localtime" ]; then
		ofile=${file:1}
		[ "$ofile" = "/usr/bin/[[" -o "$ofile" = "/usr/bin/[" ] && continue
		grep -q "^$ofile$" origfiles.txt && continue ## Skip original files and only copy latest ones
	fi
	[ ! -d "$file" ] && { dir=${file%/*}; mkdir -p "$tdir/$dir"; cp -aulv "$file" "$tdir/$file"; };
done
# Copy changed files
md5sum -c origmd5sums.txt 2>/dev/null | grep -v "FAILED open or read" | egrep -vi ": OK$|qtopia|/usr/share/zoneinfo|/root" | sed -e 's/: FAILED$//g' | while read changed; do
	dir=${changed%/*}; mkdir -p "$tdir/$dir"; cp -aulv "$changed" "$tdir/$changed";
done
cd "$tdir"

#makeself --bzip2 --current . "$HOME/play3abn.run" $(date +"play3abn-%Y%m%d-%H%M%S") "./play3abn/arminstall.sh"
#makeself --gzip --current . "$OUT" $(date +"play3abn-%Y%m%d-%H%M%S") "./play3abn/arminstall.sh"

# NOTE: Compression turned off so rsync can use its algorithm (and compression)
makeself --nocomp --current . "$OUT" $(date +"play3abn-%Y%m%d-%H%M%S") "./play3abn/arminstall.sh"
echo "Fixing for busybox df..."
sed -e 's@df -kP "$1"@df -kP | grep " /"$@g' > "$OUT.tmp" < "$OUT" && mv "$OUT.tmp" "$OUT" && chmod 755 "$OUT"
sudo cp -v "$OUT" /home/radioupdates/play3abn.run
echo rsync --group -P --progress -avz -e "ssh -l radioupdates" "$OUT" radio.iglooware.com:/home/radioupdates/
rsync --group -P --progress -avz -e "ssh -l radioupdates" "$OUT" radio.iglooware.com:/home/radioupdates/
echo "Done."
