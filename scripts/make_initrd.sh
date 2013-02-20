#!/bin/bash
STARTWD=$(pwd)
script_dir=$(dirname "$0")
cd "$script_dir"
rm ../test/myinitrd.gz
cd ../initramfs
find . | cpio --create --format='newc' > ../test/initramfs
cd ..
gzip test/iniramfs
chmod 777 test/initramfs.gz
cd "$STARTWD"
