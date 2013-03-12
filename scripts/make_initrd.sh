#!/bin/bash
STARTWD=$(pwd)
script_dir=$(dirname "$0")
cd "$script_dir"
rm ../test/initramfs.gz
cd ../kernel_chooser/initramfs
find . | cpio --create --format='newc' > ../../test/initramfs
cd ../..
gzip test/initramfs
chmod 777 test/initramfs.gz
cd root_chooser/initramfs
find . | cpio --create --format='newc' > ../initrd
cd ..
gzip initrd
chmod 777 initrd.gz
cd "$STARTWD"
