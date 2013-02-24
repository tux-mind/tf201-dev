#!/bin/bash
if [ "$1" ]; then
	blob=$(find "$1" -iname "*.blob" | tail -n1);
	echo "going to flash $blob"
	fastboot -i 0x0b05 flash boot "$blob" && \
	fastboot -i 0x0b05 reboot
	exit
fi
script_dir=$(dirname $0)
cd "$script_dir"
abootimg --create "../test/test.LNX" -k "../test/zImage" -r "../test/initramfs.gz" && \
blobpack "../test/test.blob" LNX "../test/test.LNX" && \
fastboot -i 0x0b05 flash boot "../test/test.blob" && \
fastboot -i 0x0b05 reboot
cd -
