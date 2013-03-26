# TOOLCHAIN="" and TOOLCHAIN_DIR="" configure the compiler.
# DEVELOPMENT=1 will enable various options such as debug statements and extra pauses.

TARGET_BIN=kernel_chooser
INITRD_DIR=initramfs
INITRD=initrd

CFLAGS=-Wall -Werror -g -static

ifdef TOOLCHAIN_DIR
	CFLAGS+=-I$(TOOLCHAIN_DIR)/include
	LDFLAGS=-L$(TOOLCHAIN_DIR)/lib
else
	unexport LDFLAGS
endif

LDFLAGS+=-lz -llzma -lmenu -lcurses

ifndef TOOLCHAIN
	TOOLCHAIN=arm-unknown-linux-gnueabi
endif

CC=$(TOOLCHAIN)-gcc
LD=$(TOOLCHAIN)-ld

DEVELOPMENT ?= 1
ifeq (DEVELOPMENT, 1)
    CFLAGS = $(CFLAGS) -DDEVELOPMENT
endif

all: v2 initrd use_initrd

v2: kernel_chooser2.c menu3.o nGUI.o kexec.o lzma.o zlib.o sha256.o
	$(CC) $(CFLAGS) -o $(TARGET_BIN) $? $(LDFLAGS)

v1: kernel_chooser.c menu2.o kexec.c zlib.o lzma.o sha256.o
	$(CC) $(CFLAGS) -o $(TARGET_BIN) $? $(LDFLAGS)

%.o: %.c %.h common2.h
	$(CC) $(CFLAGS) -c -o $@ $<

initrd: $(TARGET_BIN) $(INITRD_DIR)
	cp $(TARGET_BIN) $(INITRD_DIR)/init
	cd $(INITRD_DIR); find . | cpio --create --format='newc' > ../$(INITRD); gzip -f ../$(INITRD)

use_initrd:
	cp $(INITRD).gz ../test/initramfs.gz
	../scripts/reflash_kernel.sh

clean:
	rm -f $(TARGET_BIN) *.o
