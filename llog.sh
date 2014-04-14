llog=$(find /tmp/play3abn/tmp/logs/ -maxdepth 1 -mindepth 1 -type d | sort | tail -n 1); less "$llog/out-playloop.sh.log.txt"
