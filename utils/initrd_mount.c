#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <zlib.h>
#include <sys/mount.h>
#include <errno.h>

#include "initrd_mount.h"

/** extract cpio contained in cpio_buffer in dst folder
 * @cpio_buffer: the cpio archive
 * @len: the length of the cpio_buffer
 * @dst: the directory to extract the archive into
 */
int cpio_copyin(char *cpio_buffer, unsigned long len, const char *dst)
{
	pid_t pid;
	int pipefd[2];

	if(pipe(pipefd))
		return -1;

	if(!(pid = fork()))
	{
		char *cpio_argv[] = CPIO_ARGV;

		free(cpio_buffer);
		close(pipefd[1]);

		if(chdir(dst))
			exit(-1);
		dup2(pipefd[0],0);
		execv(cpio_argv[0],cpio_argv);
		exit(-1);
	}
	else if(pid < 0)
		return -1;
	else
	{
		close(pipefd[0]);
		write(pipefd[1],cpio_buffer,len);
		close(pipefd[1]);
		// HACK: use pipefd[0] to store exit status
		wait(&pipefd[0]);
	}
	if(WIFEXITED(pipefd[0]) && WEXITSTATUS(pipefd[0]) == EXIT_SUCCESS)
		return 0;
	return -1;
}

/** extract an initrd image into a folder
 * @file: path to the initrd file
 * @dest: the directory where we extract to
 */
int initrd_extract(char *file, const char *dest)
{
	struct stat st;
	int fd;
	unsigned long len,readed;
	char magic[6],buff[1024],*cpio_archive;

	if(stat(file,&st))
		return -1;
	if(!S_ISREG(st.st_mode))
	{
		errno = EINVAL;
		return -1;
	}
	cpio_archive=NULL;
	fd = open(file,O_RDONLY);
	if(fd<0)
		return -1;
	if((len = read(fd,magic,6)) != 6)
	{
		close(fd);
		return -1;
	}
	// gzip magic
	if(magic[0] == 0x1f && magic[1] == 0x8b)
	{
		close(fd);
		cpio_archive = zlib_decompress_file(file,&len);
		if(!cpio_archive)
			return -1;
		memcpy(magic,cpio_archive,6);
	}
	else // get file size
	{
		while((readed = read(fd,buff,1024)) > 0)
			len+=readed;
		close(fd);
		if(readed)
			return -1;
	}
	// check cpio magic
	if(strncmp(magic,"07070",5) || (magic[5] != '7' && magic[5] != '1' && magic[5] != '2'))
	{
		errno = EINVAL;
		return -1;
	}
	if(!cpio_archive) // we have to read it
	{
		cpio_archive = malloc(len); // len is the size in bytes ( man 2 read )
		if(!cpio_archive)
		{
			return -1;
		}
		fd = open(file,O_RDONLY);
		if(fd<0)
		{
			free(cpio_archive);
			return -1;
		}
		if((readed = read(fd,cpio_archive,len)) != len)
		{
			free(cpio_archive);
			close(fd);
			return -1;
		}
		close(fd);
	}
	if(cpio_copyin(cpio_archive,len,dest))
	{
		free(cpio_archive);
		errno = ECHILD;
		return -1;
	}
	free(cpio_archive);
	return 0;
}

/**
 * try to mount an initrd on mountpoint
 * @file: the initrd file
 * @mountpoint: the mountpoint on which we will mount the tmpfs
 */
int initrd_mount(char *file, const char *mountpoint)
{
	umount(mountpoint);
	if(mount("tmpfs",mountpoint,"tmpfs",MS_RELATIME,""))
		return -1;
	if(initrd_extract(file,mountpoint))
	{
		umount(mountpoint);
		return -1;
	}
	return 0;
}
