#!/sbin/sh
if [ -e /data/android_system ]; then
  echo "[system_mounter] mount" >> /dev/kmsg
  mount -text4 /data/android_system /system
  echo "[system_mounter] done ($?)" >> /dev/kmsg
else
  echo "[system_mounter] mount" >> /dev/kmsg
  mount -text4 /dev/block/mmcblk0p1 /system
  echo "[system_mounter] done ($?)" >> /dev/kmsg
fi