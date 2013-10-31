# DEVELOPMENT=1 will enable various options such as debug statements and extra pauses. (defaults to 0)
DEVELOPMENT?=0

TARGET_BIN=kernel_chooser
INITRD_DIR=initramfs
INITRD=initrd
UTILS=../utils/

CC?=arm-unknown-linux-gnueabi-gcc
LD?=arm-unknown-linux-gnueabi-ld
CFLAGS=-Wall -Werror -g -static -I$(UTILS)
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

all: kernel_chooser initrd

kernel_chooser: kernel_chooser.c menu.o fbGUI.o nGUI.o kexec.o $(UTILS)lzma.o $(UTILS)zlib.o $(UTILS)sha256.o
	$(CC) $(CFLAGS) -o $(TARGET_BIN) $? $(LDFLAGS)

%.o: %.c %.h common.h
	$(CC) $(CFLAGS) -c -o $@ $<

initrd: $(TARGET_BIN) $(INITRD_DIR)
	cp $(TARGET_BIN) $(INITRD_DIR)/init
	cd $(INITRD_DIR); find . | cpio --create --format='newc' > ../$(INITRD); gzip -f ../$(INITRD)

use_initrd:
	if [ ! -d ../test ]; then mkdir ../test; fi
	cp $(INITRD).gz ../test/initramfs.gz
	../scripts/reflash_kernel.sh

use_recovery:
	../scripts/make_recovery_zip.sh

clean:
	rm -f $(TARGET_BIN) *.o
