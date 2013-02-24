#!/bin/bash
STARTWD=$(pwd)
script_dir=$(dirname "$0")
cd "$script_dir"
rm ../test/initramfs.gz
cd ../initramfs
find . | cpio --create --format='newc' > ../test/initramfs
cd ..
gzip test/initramfs
chmod 777 test/initramfs.gz
cd "$STARTWD"
