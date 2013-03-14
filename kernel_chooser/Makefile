# may require edits to reflect your toolchain and compiler options
# -static is required to build standalone executables
# -Werror forces you to write good code

CC?=arm-unknown-linux-gnueabi-gcc
LD=arm-unknown-linux-gnueabi-ld
CFLAGS=-Wall -Werror -g -static
LDFLAGS=-lz -llzma
TARGET_BIN=kernel_chooser
INITRD_DIR=initramfs
INITRD=initrd

all: v1 initrd use_initrd

v1: kernel_chooser.c menu2.o kexec.c zlib.o lzma.o sha256.o
	$(CC) $(CFLAGS) -o $(TARGET_BIN) $? $(LDFLAGS)

%.o: %.c common.h
	$(CC) $(CFLAGS) -c -o $@ $<

initrd: $(TARGET_BIN) $(INITRD_DIR)
	cp $(TARGET_BIN) $(INITRD_DIR)/init
	# we have to run this on a single line ( make run a shell per line O.o )
	cd $(INITRD_DIR); find . | cpio --create --format='newc' > ../$(INITRD); gzip -f ../$(INITRD)

use_initrd:
	cp $(INITRD).gz ../test/initramfs.gz
	../scripts/reflash_kernel.sh


clean:
	rm -f $(TARGET_BIN) *.o
