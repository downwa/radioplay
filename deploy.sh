#!/bin/sh

olddeploy() {
reldate=$(date +"%Y%m%d%H%M%S")
echo "SUMMING releases for $reldate..."
for platform in $(cd fullroot/; ls); do
	[ ! -d "fullroot/$platform" ] && continue
	cd "fullroot/$platform"
	[ "$platform" != "filler" ] && echo "$reldate" >play3abn/vars/release.txt
	../md5sums.sh
	release=$(md5sum md5sums.txt | cut -d ' ' -f 1)
	printf "%-20s %s\n" "$platform" "$release"
	cd ../..
done
echo "Fixing local release..."
mkdir -p /tmp/play3abn/vars/
sudo chgrp www-data /tmp/play3abn/vars/
touch /tmp/play3abn/vars/release_progress.txt
sudo rm -fv /tmp/release-*

read -p "Deploy to iglooware server? " yn
[ "$yn" = "n" ] && return
echo "Fixing remote release..."
ssh -t -t wdowns@radio.iglooware.com sudo rm -fv /tmp/release-*

echo "DEPLOYING releases to iglooware server:"
}

olddeploy
#./deployapp.sh
./deploysite.sh
./deployself.sh
