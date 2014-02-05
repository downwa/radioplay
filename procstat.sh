#!/bin/sh

watch=$1
[ "$watch" = "" ] && echo "Missing name of process to check" && exit 1

total_jiffies_1=$(cat /proc/stat | grep "^cpu " | sed -e 's/cpu *//g' -e 's/ /+/g' | bc)
work_jiffies_1=$(cat /proc/stat | grep "^cpu " | sed -e 's/cpu *//g' -e 's/ /+/g' | cut -d '+' -f -3 | bc)
time1=$(cat /proc/$(pidof $watch)/stat | awk '{print $14"+"$15}' | bc)

sleep 1

total_jiffies_2=$(cat /proc/stat | grep "^cpu " | sed -e 's/cpu *//g' -e 's/ /+/g' | bc)
work_jiffies_2=$(cat /proc/stat | grep "^cpu " | sed -e 's/cpu *//g' -e 's/ /+/g' | cut -d '+' -f -3 | bc)
time2=$(cat /proc/$(pidof $watch)/stat | awk '{print $14"+"$15}' | bc)

work_over_period=$((${work_jiffies_2}-${work_jiffies_1}))
total_over_period=$((${total_jiffies_2}-${total_jiffies_1}))
pctcpu=$((${work_over_period}*100/${total_over_period}))

workproc=$((${time2}-${time1}))
pctproc=$((10000*$workproc/${total_over_period}))

echo "$pctproc hundredths of % of $pctcpu%"
