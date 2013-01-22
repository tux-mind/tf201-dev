abootimg --create ./test/test.LNX -k ./kernel/zImage -r ./test/myinitrd.gz && blobpack test/test.blob LNX test/test.LNX
fastboot -i 0x0b05 flash boot test/test.blob
