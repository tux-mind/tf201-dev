CC=arm-unknown-linux-gnueabi-gcc
CFLAGS=-Wall -g -static

all: boot_chooser android_init linux_init


boot_chooser: boot_chooser.c
	$(CC) $(CFLAGS) -o $@ $<

android_init: android_init.c
	$(CC) $(CFLAGS) -o $@ $<

linux_init: linux_init.c
	$(CC) $(CFLAGS) -o $@ $<