CC=arm-unknown-linux-gnueabi-gcc
LD=arm-unknown-linux-gnueabi-ld
CFLAGS=-Wall -g -static
TARGET_BIN=boot_chooser

all: v3

v3: boot_chooser3.c loop_mount.o
	$(CC) $(CFLAGS) -o $(TARGET_BIN) $?

v2: boot_chooser2.c
	$(CC) $(CFLAGS) -o $(TARGET_BIN) $<

v1: boot_chooser.c
	$(CC) $(CFLAGS) -o $(TARGET_BIN) $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<