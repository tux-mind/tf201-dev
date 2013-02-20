#!/bin/bash
script_dir=$(dirname $0)
cd "$script_dir"
abootimg --create ../test/test.LNX -k ../test/zImage -r ../test/initramfs.gz && \
blobpack ../test/test.blob LNX ../test/test.LNX && \
fastboot -i 0x0b05 flash boot ../test/test.blob && \
fastboot -i 0x0b05 reboot
cd -
