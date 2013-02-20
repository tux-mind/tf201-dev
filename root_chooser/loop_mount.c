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

/** COPYING NOTES
 * the following 3 functions contains code taken from
 * util-linux-ng:mount heavy shrinked because i known we run linux >= 3.1
 * this tell us that kernel have loop autoclear support.
 * TODO: is loop_info64_to_old needed ?
 */

#include "loop_mount.h"

#define MS_LOOP 0x00010000

static int
loop_info64_to_old(const struct loop_info64 *info64, struct loop_info *info)
{
        memset(info, 0, sizeof(*info));
        info->lo_number = info64->lo_number;
        info->lo_device = info64->lo_device;
        info->lo_inode = info64->lo_inode;
        info->lo_rdevice = info64->lo_rdevice;
        info->lo_offset = info64->lo_offset;
        info->lo_encrypt_type = info64->lo_encrypt_type;
        info->lo_encrypt_key_size = info64->lo_encrypt_key_size;
        info->lo_flags = info64->lo_flags;
        info->lo_init[0] = info64->lo_init[0];
        info->lo_init[1] = info64->lo_init[1];
        if (info->lo_encrypt_type == LO_CRYPT_CRYPTOAPI)
                memcpy(info->lo_name, info64->lo_crypt_name, LO_NAME_SIZE);
        else
                memcpy(info->lo_name, info64->lo_file_name, LO_NAME_SIZE);
        memcpy(info->lo_encrypt_key, info64->lo_encrypt_key, LO_KEY_SIZE);

        /* error in case values were truncated */
        if (info->lo_device != info64->lo_device ||
            info->lo_rdevice != info64->lo_rdevice ||
            info->lo_inode != info64->lo_inode ||
            info->lo_offset < 0 ||
	    (uint64_t) info->lo_offset != info64->lo_offset)
                return -EOVERFLOW;

        return 0;
}

int set_loop(const char *device, char *file,int *fd_to_close)
{
	struct loop_info64 loopinfo64;
	int fd, ffd, i, old_errno;

	if ((ffd = open(file, O_RDWR)) < 0)
		return 1;
	else if ((fd = open(device, O_RDWR)) < 0)
	{
		close(ffd);
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
			return 1;
	}
	close (ffd);

	i = ioctl(fd, LOOP_SET_STATUS64, &loopinfo64);
	if (i) {
		struct loop_info loopinfo;
		old_errno = errno;

		i = loop_info64_to_old(&loopinfo64, &loopinfo);
		if (i) {
			close(fd);
			errno = old_errno;
			return 1;
		} else {
			i = ioctl(fd, LOOP_SET_STATUS, &loopinfo);
			if (i)
			{
				close(fd);
				errno = old_errno;
				return 1;
			}
		}
		memset(&loopinfo, 0, sizeof(loopinfo));
	}
	memset(&loopinfo64, 0, sizeof(loopinfo64));

	if (i) {
		ioctl (fd, LOOP_CLR_FD, 0);
		close (fd);
		errno = old_errno;
		return 1;
	}
	*fd_to_close = fd;
	return 0;
}

/** this function check if the regular file loopfile can be mounted thought loop devices.
 * @loopfile:			the source file to mount. we son't support offset, so it MUST be an ext4 partition.
 * @loopdev:			the loop device where we want to setup the loopfile.
 * @flags:				pointer to the mount flags. we need to modify these for mount loop devices.
 * @fd_to_close:	used for keep trace of the leaked fd of set_loop().
 */

int loop_check(char *loopfile, const char *loopdev, int *flags, int *fd_to_close)
{
	int res,retries;
	struct stat st;

	if (stat(loopfile, &st) == 0 && S_ISREG(st.st_mode))
  {
    *flags |=  MS_LOOP;
		for(retries = 0; (res = set_loop(loopdev,loopfile,fd_to_close)) == 2 && retries < 3; retries++)
			sleep(1);
		if(res)
			return 1;
		res = strlen(loopdev);
		strncpy(loopfile,loopdev,res+1);
  }
  return 0;
}

