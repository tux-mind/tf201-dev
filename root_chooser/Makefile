CC=arm-unknown-linux-gnueabi-gcc
LD=arm-unknown-linux-gnueabi-ld
CFLAGS=-Wall -Werror -g -static
LDFLAGS= -llzma -lz
TARGET_BIN=root_chooser

all: v6

v6: root_chooser6.c menu2.o lzma.o zlib.o sha256.o kexec.o
	$(CC) $(CFLAGS) -o $(TARGET_BIN) $? $(LDFLAGS)

v5: root_chooser5.c loop_mount2.o menu.o
	$(CC) $(CFLAGS) -o $(TARGET_BIN) $?

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

clean:
	rm $(TARGET_BIN) *.o
