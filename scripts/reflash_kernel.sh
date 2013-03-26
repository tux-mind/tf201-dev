#!/bin/bash

#export NVFLASH if you want to use that instead of fastboot

flash() {
	if [ -n $2 ]; then
		wheelie --blob ./blob.bin && \
		nvflash -r --download 6 $1 && \
		nvflash -r --go && \
		exit 0
	else
		fastboot -i 0x0b05 flash boot $1 && \
		fastboot -i 0x0b05 reboot && \
		exit 0
	fi
	exit 1
}

script_dir=$(dirname $0)
cd "$script_dir"

if [ "$1" ]; then
	blob=$(find "$1" -iname "*.blob" | tail -n1);
	echo "going to flash $blob"
	flash $blob
	exit
fi

abootimg --create "../test/test.LNX" -k "../test/zImage" -r "../test/initramfs.gz"

if [ "$NVFLASH" ]; then
	flash ../test/test.LNX 1 && \
	cd -
else

	blobpack "../test/test.blob" LNX "../test/test.LNX" && \
	flash ../test/test.blob && \
	cd -
fi