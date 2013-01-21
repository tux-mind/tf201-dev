./test/abootimg/abootimg --create ./test/test.LNX -k ./test/zImage -r ./test/myinitrd.gz && ./BlobTools/blobpack test/test.blob LNX test/test.LNX
fastboot -i 0x0b05 flash boot test/test.blob
