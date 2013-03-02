CC=arm-unknown-linux-gnueabi-gcc
LD=arm-unknown-linux-gnueabi-ld
CFLAGS=-Wall -g -static
TARGET_BIN=root_chooser

all: v4

v4: root_chooser4.c loop_mount4.o
	$(CC) $(CFLAGS) -o $(TARGET_BIN) $?

v3: root_chooser3.c loop_mount.o
	$(CC) $(CFLAGS) -o $(TARGET_BIN) $?

v2: root_chooser2.c
	$(CC) $(CFLAGS) -o $(TARGET_BIN) $<

v1: root_chooser.c
	$(CC) $(CFLAGS) -o $(TARGET_BIN) $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
