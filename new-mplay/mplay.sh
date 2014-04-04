#!/bin/sh

DIR="/tmp/play3abn"
PLAY="$DIR/play.fifo"

mkdir -p "$DIR"
rm -f "$PLAY"
mkfifo "$PLAY"

#find /sdcard/filler/songs/ -name '*.ogg' | while read file; do ogg123 -d raw -f - "$file"; done | aplay -f S16_LE -c1 -r16000

playback() {
	while [ true ]; do ./infinitepipe | aplay -f S16_LE -c1 -r16000; done
}

playback
