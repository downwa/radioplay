#!/bin/sh

echo "HOME=$HOME."
echo "Running as " $(whoami)
[ "$HOME" = "/" ] && export HOME="/home/pi"
cd "$HOME/radioplay"

sudo rm -f /tmp/play3abn/play3abn.log

setterm -powersave off 2>/dev/null
xset s off 2>/dev/null
setterm -blank 0 -powerdown 0

# Refresh will wait until program lists are available, run once, then exit.
./refresh-programlist2.sh &

./timekeep.sh &
sleep 1
./syncradio.sh &
./failsafenet.sh &
./progress.sh &

date +"%D %H:%M:%S make installpi"
make installpi

ARCH=$(arch)
cd "$ARCH"
date +"%D %H:%M:%S playradio.sh"
./play3abn
