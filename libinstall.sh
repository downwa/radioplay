#!/bin/sh

bash=$(stat -c "%N" /bin/sh | cut -d '`' -f 3- | cut -d "'" -f 1)
[ "$bash" != "/bin/bash" ] && sudo ln -sf /bin/bash /bin/sh
# Removed software-center from pkgs
pkgs="make g++ libcurl4-openssl-dev libvorbisidec-dev libncurses5-dev libmad0-dev madplay alsa-oss realpath libpulse-dev libpcre3-dev"
count=$(echo "$pkgs" | wc -w)
inst=$(dpkg -s $pkgs | grep Status:.*ok.installed | wc -l)
if [ "$inst" -lt "$count" ]; then # Install the missing ones
	missing=$(for pkg in $pkgs; do dpkg -s "$pkg" | grep -q Status:.*ok.installed || echo $pkg; done)
	sudo apt-get install $missing
fi
#sh ../bin/work3abn
