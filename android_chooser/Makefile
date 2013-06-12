TARGET_BIN=android_chooser
INITRD_DIR=initramfs
INITRD=initrd
WORKING_DIR=.android_chooser

CC?=arm-unknown-linux-gnueabi-gcc
LD?=arm-unknown-linux-gnueabi-ld
CFLAGS=-Wall -Werror -g -static -I../utils
LDFLAGS=-lz

ifdef INCLUDE_DIR
	CFLAGS:=$(CFLAGS) -I$(INCLUDE_DIR)
endif

ifdef LIB_DIR
	LDFLAGS:=-L$(LIB_DIR) $(LDFLAGS)
endif

all: android_chooser initrd

android_chooser: android_chooser.c ../utils/loop_mount.o mountpoints.o ../utils/initrd_mount.o ../utils/zlib.o
	$(CC) $(CFLAGS) $? $(LDFLAGS) -o $(TARGET_BIN)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
	
initrd:
	cp $(TARGET_BIN) $(INITRD_DIR)/$(WORKING_DIR)/init
	ln -fs "$(WORKING_DIR)/init" $(INITRD_DIR)/init
	cd $(INITRD_DIR); find . | cpio --create --format='newc' > ../$(INITRD); gzip -f ../$(INITRD)

clean:
	rm -f $(TARGET_BIN) *.o
