# DEVELOPMENT=1 will enable various options such as debug statements and extra pauses. (defaults to 0)
DEVELOPMENT?=0

TARGET_BIN=kernel_chooser
INITRD_DIR=initramfs
INITRD=initrd

CC?=arm-unknown-linux-gnueabi-gcc
LD?=arm-unknown-linux-gnueabi-ld
CFLAGS=-Wall -Werror -g -static
LDFLAGS=-lz -llzma -lmenu -lcurses

ifeq ($(DEVELOPMENT), 1)
    CFLAGS+=-DDEVELOPMENT
endif

ifdef INCLUDE_DIR
	CFLAGS+=-I$(INCLUDE_DIR)
endif

ifdef LIB_DIR
	LDFLAGS:=-L$(LIB_DIR) $(LDFLAGS)
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
