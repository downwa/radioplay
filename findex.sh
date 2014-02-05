#!/bin/sh

# Find exceptional queued items
cd /tmp/play3abn/tmp/playq
find -type l | sort | while read name; do [ "${name:0:21}" = "${oname:0:21}" -a "${name:0:28}" != "${oname:0:28}" ] && echo "COMP: $nname; $oname; $name"; oname=$name; done | less
