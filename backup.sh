#!/bin/sh

file=$(date +"$HOME/dev/3abn/backup/play3abn-%Y%m%d-%H%M%S.tar.bz2")
cd ../
textfiles=$(file -i $(find play3abn/ -maxdepth 1 -type f) | grep text/ | cut -d ':' -f 1)
tar cvfj "$file" $textfiles
#read -p "Iglooware Password (blank to skip upload): " password
#if [ "$password" != "" ]; then
#	base=$(basename "$file")
#        ../i386/ftp -u ftp://wdowns:$password@iglooware.com/backup/$base "$file"
#fi

