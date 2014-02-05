#!/bin/sh

while [ true ]; do clear; for file in /tmp/play3abn/curlogs/mark-*; do basename "$file"; cat "$file"; done; sleep 1; done
