
[ "$1" = "" ] && echo "Missing FillerPlayList-*.txt" 1>&2 && exit 1

cat "$1" | egrep -v "FullBreak|AfflBreak" | cut -d '~' -f -1 | sed -e 's/[0-9][a-b]*$//g' -e 's/-$//g' -e 's/[0-9]*$//g' | while read line; do code=$(echo "$line" | awk '{print $4}'); category=$(grep "^$code " CodeCategories.txt | cut -d ' ' -f 2); [ "$category" != "" ] && line=$(echo "$line" | awk '{print $1" "$2" "$3}'); echo "$line $category"; done | sed -e 's/Network_ID_with_DTMF_#4.ogg/StationID/g' -e 's/3ABN_TODAY-.*-.*-.*_-3ABN_TODAY_SIMULCAST_WITH_3ABN_TV.ogg/TESTIMONY/g' -e 's/One_Second_Time_Fill/TESTIMONY/g' -e 's/:01 [0-9]* TESTIMONY/:00 3585 TESTIMONY/g' | grep -v ":00 0001 FILL"
