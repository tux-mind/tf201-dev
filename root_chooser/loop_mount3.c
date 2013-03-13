/*
 * mount(8) -- mount a filesystem
 *
 * Copyright (C) 2011 Red Hat, Inc. All rights reserved.
 * Written by Karel Zak <kzak@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

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

#include "loop_mount3.h"

#define MS_LOOP 0x00010000

/** COPYING NOTES
 * the following 3 functions contain code taken from
 * util-linux-ng:mount heavily shrinked because i known we run linux >= 3.1
 * this tells us that kernel has loop autoclear support.
 * TODO: is loop_info64_to_old needed ?
 */

int set_loop(const char *device, char *file,int *fd_to_close)
{
	struct loop_info64 loopinfo64;
	int fd, ffd;

	if ((ffd = open(file, O_RDWR)) < 0)
	{
		ERROR("cannot open \"%s\" - %s\n",file,strerror(errno));
		return 1;
	}
	else if ((fd = open(device, O_RDWR)) < 0)
	{
		close(ffd);
		ERROR("cannot open \"%s\" - %s\n",device,strerror(errno));
		return 1;
	}
	memset(&loopinfo64, 0, sizeof(loopinfo64));
	strncpy((char *)loopinfo64.lo_file_name,file,LO_NAME_SIZE);
	loopinfo64.lo_flags = LO_FLAGS_AUTOCLEAR;

	if (ioctl(fd, LOOP_SET_FD, ffd) < 0)
	{
		close(fd);
		close(ffd);
		if (errno == EBUSY)
			return 2;
		else
		{
			ERROR("cannot associate \"%s\" with \"%s\" - %s\n",file,device,strerror(errno));
			return 1;
		}
	}
	close (ffd);

	if (ioctl(fd, LOOP_SET_STATUS64, &loopinfo64))
	{
		ERROR("ioctl: LOOP_SET_STATUS64 - %s\n",strerror(errno));
		ioctl (fd, LOOP_CLR_FD, 0);
		close (fd);
		return 1;
	}
	*fd_to_close = fd;
	return 0;
}

/** this function tries to mount the regular file loopfile on mountpoint though loop devices.
 * @loopfile:			the source file to mount. we don't support offset, so it MUST be an ext4 partition.
 * @mountpoint:		the directory were to mount the image file.
 */

int try_loop_mount(char **loopfile, const char *mountpoint)
{
	int res,retries,fd_to_close;
	struct stat st;

	/** @fd_to_close explaitation
	 * if we use loop devices we have to
	 * keep the /dev/loop* file descriptor
	 * until mount(2) call is done.
	 * BUT we also have to close it after mount is done.
	 * int the mount(8) source code i found that
	 * they don't close that file descriptor at all.
	 * their reason is that mount(8) is a short-lived process,
	 * so exit() call will close that fd automatically.
	 * here we are the init process, the longest-lived process! :)
	 * so, fd_to_close is the /dev/loop file descriptor to close
	 * after the related mount(2) call.
	 */

	if(stat(*loopfile, &st))
	{
		ERROR("cannot stat \"%s\" - %s\n",*loopfile,strerror(errno));
		return 1;
	}

	if (S_ISREG(st.st_mode))
  {
		fd_to_close=0;
		for(retries = 0; (res = set_loop(LOOP_DEVICE,*loopfile,&fd_to_close)) == 2 && retries < 3; retries++)
			sleep(1);
		if(res)
			return 1;
		umount(mountpoint);
		if(mount(LOOP_DEVICE,mountpoint,"ext4",MS_LOOP,""))
		{
			ERROR("cannot mount \"%s\" on \"%s\" - %s\n",LOOP_DEVICE,mountpoint,strerror(errno));
			close(fd_to_close);
			return 1;
		}
		close(fd_to_close);
		free(*loopfile);
		res = strlen(mountpoint);
		*loopfile = malloc((res+1)*sizeof(char));
		if(!*loopfile)
		{
			FATAL("malloc - %s\n",strerror(errno));
			return 1;
		}
		strncpy(*loopfile,mountpoint,res);
		*(*loopfile+res)='\0';
  }

  return 0;
}
