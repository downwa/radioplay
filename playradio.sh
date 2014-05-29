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
nohup ./refresh-programlist2.sh 2>/tmp/refresh.err >/dev/null &
nohup ./timekeep.sh    2>/tmp/timekeep.err >/dev/null &
sleep 1
nohup ./syncradio.sh   2>/tmp/syncradio.err >/tmp/syncradio.out &
nohup ./failsafenet.sh 2>/tmp/failsafenet.err >/dev/null &
nohup ./progress.sh    2>/dev/null >/dev/null &
nohup ./alive.sh       2>/dev/null >/dev/null &

date +"%D %H:%M:%S make installpi"
make installpi

ARCH=$(arch)
cd "$ARCH"
date +"%D %H:%M:%S playradio.sh"
./play3abn
