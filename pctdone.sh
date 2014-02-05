tail -f /tmp/xarchive.log | while read line; do v1=$(echo "$line" | awk '{print $11}'); v2=$(echo "$line" | awk '{print $13}'); pct=$(($v1*100/$v2)); printf " %s%% done    \r"
 "$pct"; sleep 1; done
