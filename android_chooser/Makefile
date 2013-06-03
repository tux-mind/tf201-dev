TARGET_BIN=android_chooser
INITRD_DIR=initramfs
INITRD=initrd

CC?=arm-unknown-linux-gnueabi-gcc
LD?=arm-unknown-linux-gnueabi-ld
CFLAGS=-Wall -Werror -g -static
LDFLAGS=-lz

ifdef INCLUDE_DIR
	CFLAGS:=$(CFLAGS) -I$(INCLUDE_DIR)
endif

ifdef LIB_DIR
	LDFLAGS:=-L$(LIB_DIR) $(LDFLAGS)
endif

all: v1 initrd

v1: android_chooser.c loop_mount3.o mountpoints.o initrd_mount.o zlib.o
	$(CC) $(CFLAGS) $? $(LDFLAGS) -o $(TARGET_BIN)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

initrd:
	cp $(TARGET_BIN) $(INITRD_DIR)/init
	cd $(INITRD_DIR); find . | cpio --create --format='newc' > ../$(INITRD); gzip -f ../$(INITRD)

clean:
	rm -f $(TARGET_BIN) *.o
