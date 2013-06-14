TARGET_BIN=android_chooser
INITRD_DIR=initramfs
INITRD=initrd
WORKING_DIR=.android_chooser
UTILS=../utils

CC?=arm-unknown-linux-gnueabi-gcc
LD?=arm-unknown-linux-gnueabi-ld
CFLAGS=-Wall -Werror -g -static -I$(UTILS)
LDFLAGS=-lz

ifdef INCLUDE_DIR
	CFLAGS:=$(CFLAGS) -I$(INCLUDE_DIR)
endif

ifdef LIB_DIR
	LDFLAGS:=-L$(LIB_DIR) $(LDFLAGS)
endif

all: android_chooser initrd

android_chooser: android_chooser.c $(UTILS)/loop_mount.o mountpoints.o $(UTILS)/initrd_mount.o $(UTILS)/zlib.o $(UTILS)/detect_fs.o
	$(CC) $(CFLAGS) $? $(LDFLAGS) -o $(TARGET_BIN)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
	
initrd:
	cp $(TARGET_BIN) $(INITRD_DIR)/$(WORKING_DIR)/init
	ln -fs "$(WORKING_DIR)/init" $(INITRD_DIR)/init
	cd $(INITRD_DIR); find . | cpio --create --format='newc' > ../$(INITRD); gzip -f ../$(INITRD)

clean:
	rm -f $(TARGET_BIN) *.o
