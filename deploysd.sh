#!/bin/sh

[ "$1" = "" ] && echo "Missing IP address to deploy to." && exit 1

cat "$HOME/play3abn.run" | nc "$1" 1234
