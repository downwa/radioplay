#!/bin/sh

#top -H -p $(pidof mplay),$(pidof src-3abn),$(pidof mdown),$(pidof mscreen),$(pidof play3abn)
echo "pcpu  pid   tid  class rtprio ni pri psr stat comm"
ps -Leo pcpu,pid,tid,class,rtprio,ni,pri,psr,stat,comm | egrep "mplay|src-3abn|mdown|mscreen|lrucache|play3abn" | sort -nr
