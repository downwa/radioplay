findTarget() {
	tgtdir=$(mount | grep /dev/mmcblk0.*/sdcard.*vfat | awk '{print $3}' | while read tgtdir; do
		[ -d "$tgtdir/schedules" ] && echo "$tgtdir" && break
	done)
}
