// fix EOVERFLOW when stat files over 2GB
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <linux/loop.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/time.h>

#define LOOP_DEVICE "/dev/loop0"
#define LOOP_DEVICE_STRLEN 10
