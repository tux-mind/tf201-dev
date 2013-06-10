TARGET_BIN=root_chooser
INITRD_DIR=initramfs
INITRD=initrd

CC?=arm-unknown-linux-gnueabi-gcc
LD?=arm-unknown-linux-gnueabi-ld
CFLAGS=-Wall -Werror -g -static -I../utils
LDFLAGS=-lz -llzma

ifdef INCLUDE_DIR
	CFLAGS:=$(CFLAGS) -I$(INCLUDE_DIR)
endif

ifdef LIB_DIR
	LDFLAGS:=-L$(LIB_DIR) $(LDFLAGS)
endif

all: root_chooser initrd

root_chooser: root_chooser.c ../utils/initrd_mount.o ../utils/loop_mount.o ../utils/zlib.o
	$(CC) $(CFLAGS) -o $(TARGET_BIN) $? $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

initrd:
	cp $(TARGET_BIN) $(INITRD_DIR)/init
	cd $(INITRD_DIR); find . | cpio --create --format='newc' > ../$(INITRD); gzip -f ../$(INITRD)

clean:
	rm -f $(TARGET_BIN) *.o
