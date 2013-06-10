CC?=arm-unknown-linux-gnueabi-gcc
LD?=arm-unknown-linux-gnueabi-ld
CFLAGS=-Wall -Werror -g -static
LDFLAGS=-lz -llzma

all: initrd_mount.o loop_mount.o zlib.o sha256.o

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o