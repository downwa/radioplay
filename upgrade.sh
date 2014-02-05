#!/bin/sh

touch md5sums.txt # In case there is no md5sums.sh
./md5sums.sh >/dev/null
sum=$(md5sum md5sums.txt | awk '{print $1}')
sudo ../i386/getarc md5sums.txt http://iglooware.com/3abnrelease/download/release-FriendlyARM_armv4tl-$sum.tgz
