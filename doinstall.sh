#!/bin/sh

# Platform derived using: platform=$(echo $(uname -s -i 2>/dev/null || uname -s -m) | sed 's/ /_/g')
# Produces: Linux_i386 or Linux_armv4tl (FriendlyARM) or Linux_armv6l (kindle) or ?

for platform in $(cd fullroot/; ls); do
	[ "$platform" = "filler" -o ! -d "fullroot/$platform" ] && continue # Not a real platform
	arch=$(echo "$platform" | cut -d '_' -f 2)
	echo "INSTALLING $platform from $arch"
	cd "$arch"
	tocopy=$(file * | egrep "ELF.*executable|LSB shared object|script.*executable" | cut -d ':' -f 1)
	for file in $tocopy; do
		diff "$file" "../fullroot/$platform/play3abn/$file" >/dev/null && continue # Skip copying unchanged files
		cp -v "$file" ../fullroot/$platform/play3abn/
	done
	pushd ../fullroot/$platform/play3abn/ >/dev/null
	md5sum $(file * | egrep "ELF.*executable|LSB shared object|script.*executable" | cut -d ':' -f 1) >files.txt
	popd >/dev/null
	cd ..
done
echo "Installing local libraries..."
cd i386/
for file in lib*.so libs/lib*.so*; do
        base=$(basename "$file")
        [ "$file" -nt /lib/i386-linux-gnu/$base ] && sudo cp -av "$file" /lib/i386-linux-gnu/
        [ -d /lib/tls/i686/cmov/ -a "$file" -nt /lib/tls/i686/cmov/$base ] && sudo cp -av "$file" /lib/tls/i686/cmov/

done
cd ..
exit 0
