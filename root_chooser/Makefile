# may require edits to reflect your toolchain and compiler options
# -static is required to build standalone executables
# -Werror forces you to write good code

CC=arm-unknown-linux-gnueabi-gcc
LD=arm-unknown-linux-gnueabi-ld
CFLAGS=-Wall -Werror -g -static
LDFLAGS=-lz
TARGET_BIN=root_chooser

all: v4

v4: root_chooser4.c loop_mount2.o initrd_mount.o  zlib.o
	$(CC) $(CFLAGS) -o $(TARGET_BIN) $? $(LDFLAGS)

v3: root_chooser3.c loop_mount.o
	$(CC) $(CFLAGS) -o $(TARGET_BIN) $?

v2: root_chooser2.c
	$(CC) $(CFLAGS) -o $(TARGET_BIN) $<

v1: root_chooser.c
	$(CC) $(CFLAGS) -o $(TARGET_BIN) $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm $(TARGET_BIN) *.o
