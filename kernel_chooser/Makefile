# may require edits to reflect your toolchain and compiler options
# -static is required to build standalone executables
# -Werror forces you to write good code

CC=arm-unknown-linux-gnueabi-gcc
LD=arm-unknown-linux-gnueabi-ld
CFLAGS=-Wall -Werror -g -static
LDFLAGS= -llzma -lz
TARGET_BIN=kernel_chooser

all: v1

v1: kernel_chooser.c menu2.o sha256.o zlib.o lzma.o kexec.o
	$(CC) $(CFLAGS) -o $(TARGET_BIN) $? $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm $(TARGET_BIN) *.o
