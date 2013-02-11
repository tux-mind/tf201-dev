/*
 * boot_chooser - v3 - choose where booting from.
 * 1) mount DATA_DEV on /data
 * 2) read the contents of /data/.boot
 * 3) parse as "block_device:root_directory:init_script"
 * 4) mount block_device on /newroot
 * 5) chroot /newroot/root_directory
 * 6) execve init_script
 *
 * ** NOTE **
 * if something goes wrong goto point 5
 * with android initramfs into newroot and
 * root_directory = "/", init_script = "/init".
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>

#define NEWROOT "/newroot/"
#define LOG "/newroot/boot_chooser.log"
#define BUSYBOX "/bin/busybox"
#define MAX_LINE 255
#define TIMEOUT 5 /* time to wait for external block devices ( USB stick ) */

// start android init at start for give ADB access
//#define ADB

#define MDEV_ARGS { "/bin/mdev","-s",NULL }

//where we looking for .boot file
#define DATA_DEV "/dev/mmcblk0p8"
//the name of the file where we read the boot option
#define BOOT_FILE "/data/.boot"

//from loop_mount.c
int loop_check(char *, const char *, int *, int *);