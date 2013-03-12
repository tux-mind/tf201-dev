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
 * roo_choooser works as follow:
 *
 * 1) mount DATA_DEV on /data
 * 2) read the contents of /data/.root(.tmp)?
 * 3) delete reded file if it's name were .root.tmp
 * 4) parse as "block_device:root_directory:init_path init_args"
 * 5) mount block_device on /newroot
 * 6) if /newroot/root_directory is a ext img mount it on /newroot
 * 7) chroot /newroot/root_directory
 * 8) execve init_script
 *
 * ** NOTE **
 * if something goes wrong or the first char
 * of the readed line is '#', goto point 7
 * with android initramfs into newroot and
 * root_directory = "/", init_path = "/init".
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

#include "common.h"
#include "menu2.h"

#define NEWROOT "/newroot/"
#define NEWROOT_STRLEN 9
#define LOG "/newroot/root_chooser.log"
#define BUSYBOX "/bin/busybox"
#define TIMEOUT 5 /* time to wait for external block devices ( USB stick ) or console */
#define INIT_MAX_ARGS 15 /* maximum number of arguments for the real init */

#if NEWROOT_STRLEN > MAX_LINE
# error "NEWROOT_STRLEN must be shorter then MAX_LINE"
#endif

// start android init at start for give ADB access
//#define ADB

#define MDEV_ARGS { "/bin/mdev","-s",NULL }

//where we looking for .root file
#define DATA_DEV "/dev/mmcblk0p8"
//the directory contains all configs
#define DATA_DIR "/data/.root.d/"
#define DATA_DIR_STRLEN 14
//the name of the file where we read the default boot options
#define ROOT_FILE "/data/.root"
//the console to use
#define CONSOLE "/dev/tty1"

#define HEADER 	"root_chooser - version 5\n"\
								"say THANKS to the 4 penguins!\n"\
								"Open Source rocks! - tux_mind <massimo.dragano@gmail.com>\n\n"

//from loop_mount4.c
//int try_loop_mount(char **, const char *);
//from kexec.c
int kexec(char *, char *, char *);