for file in /tmp/play3abn/cache/schedules1/programs/ProgramCurrent-*; do item=$(cat "$file"); printf "%-80s: %s\n" "$file" "$item"; done
