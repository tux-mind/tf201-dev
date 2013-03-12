// todo update description and project name

/*
 * root_chooser - v6 - choose the root directory.
 * Copyright (C) massimo dragano <massimo.dragano@gmail.com>
 *
 * root_chooser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * root_chooser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * root_choooser works as follows:
 *
 * 1) wait 10 seconds for the user to press a key.
       if no key is pressed, boot the default configuration
       if a key is pressed, display a menu for manual selection 
 * 2) read the contents of /data/.root.d/
 * 3) parse as "description \n blkdev:kernel:initrd \n cmdline"
 * 4) kexec hardboot into the new kernel
 * 5) the new kernel (and initrd) will then mount and boot the new system
 *
 * ** NOTE **
 * if something goes wrong the kernel will continue and boot into android
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
#include <dirent.h>
#include <sys/reboot.h>
#include <termios.h>

#include "common.h"
#include "menu2.h"

#define NEWROOT "/newroot/"
#define NEWROOT_STRLEN 9
#define LOG "/newroot/root_chooser.log"
#define BUSYBOX "/bin/busybox"
#define TIMEOUT 5 /* time to wait for external block devices ( USB stick ) or console */
#define TIMEOUT_BOOT 10 /* time to wait for the user to press a key */

#if NEWROOT_STRLEN > MAX_LINE
 #error "NEWROOT_STRLEN must be shorter then MAX_LINE"
#endif

// start android init at start to give ADB access
// TODO: is this still needed?
//#define ADB

#define MDEV_ARGS { "/bin/mdev","-s",NULL }
#define SHELL_ARGS { "/bin/sh","-s", NULL }

// the device containing DATA_DIR
#define DATA_DEV "/dev/mmcblk0p8"
// the directory contains all configs
#define DATA_DIR "/data/.root.d/"
#define DATA_DIR_STRLEN 14
// the name of the file where we read the default boot options
#define ROOT_FILE "/data/.root"
// the console to use
#define CONSOLE "/dev/tty1"

#define HEADER 	"root_chooser - version 6\n"\
				"say THANKS to the 4 penguins!\n"\
				"Open Source rocks! - tux_mind <massimo.dragano@gmail.com>\n"\
				"                   - smasher816 <smasher816@gmail.com>\n\n"

// from loop_mount4.c
//int try_loop_mount(char **, const char *);
// from kexec.c
int kexec(char *, char *, char *);