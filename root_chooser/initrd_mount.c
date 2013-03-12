#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#incldue "common.h"
#include "initrd_mount.h"

/** extract cpio contained in cpio_buffer in dst folder
 * @cpio_buffer: the cpio archive
 * @len: the length of the cpio_buffer
 * @dst: the directory to extract the archive into
 */
int cpio_copyin(char *cpio_buffer, unsigned long len, char *dst)
{
	pid_t pid;
	int pipefd[2];
	
	if(pipe(pipefd))
	{
		ERROR("pipe - %s\n",strerror(errno));
		return -1;
	}
	
	if(!(pid = fork()))
	{
		char **cpio_argv = CPIO_ARGV;
		
		free(cpio_buffer);
		close(pipefd[1]);
		
		if(chdir(dst))
		{
			ERROR("cannot chdir to \"%s\" - %s\n",dst);
			exit(-1);
		}
		dup2(pipefd[0],0);
		execv(cpio_argv[0],cpio_argv);
		exit(-1);
	}
	else if(pid < 0)
	{
		ERROR("cannot fork - %s\n",strerror(errno));
		return -1;
	}
	else
	{
		close(pipefd[0]);
		write(pipefd[1],cpio_buffer,len);
		close(pipefd[1]);
		INFO("extracting initrd\n");
		// HACK: use pipefd[0] to store exit status
		wait(&pipefd[0]);
	}
	if(WIFEXITED(pipefd[0]) && WEXITSTATUS(pipefd[0]) == EXIT_SUCCESS)
	{
		DEBUG("initrd extracted\n");
		return 0;
	}
	return -1;
}

/** try to mount an initrd on mountpoint
 * @file: path to the initrd file
 * @mountpoint: the directory where we mount to
 */
int try_initrd_mount(char **file, char *mountpoint)
{
	struct stat st;
	int fd;
	unsigned long len,readed;
	char magic[6],buff[1024],*cpio_archive;
	
	if(stat(*file,&st))
	{
		ERROR("cannot stat \"%s\" - %s\n",*file,strerror(errno));
		return -1;
	}
	if(S_ISREG(st.st_mode))
	{
		cpio_archive=NULL;
		fd = open(*file,O_RDONLY);
		if(fd<0)
		{
			ERROR("cannot open \"%s\" - %s\n",*file,strerror(errno))
			return -1;
		}
		if((len = read(fd,magic,6)) != 6)
		{
			if(len < 0)
				ERROR("cannot read \"%s\" - %s\n",*file,strerror(errno));
			else
				ERROR("file \"%s\" is too short\n",*file);
			close(fd);
			return -1;
		}
		DEBUG("first 6 bytes of file: %02x %02x %02x %02x %02x %02x\n",magic[0],magic[1],magic[2],magic[3],magic[4],magic[5],magic[6])
		// gzip magic
		if(magic[0] == 0x1f && magic[1] == 0x8b)
		{
			close(fd);
			cpio_archive = zlib_decompress_file(*file,&len);
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
			{
				ERROR("while read \"%s\" - %s\n",*file,strerror(errno));
				return -1;
			}
		}
		// cpio magic
		if(magic[0] == '0' && magic[1] == '7' && magic[2] == '0' && magic[3] == '7' && magic[4] == '0' && magic[5] == '7')
		{
			if(!cpio_archive) // we have to read it
			{
				cpio_archive = malloc(len); // len is the size in bytes ( man 2 read )
				if(!cpio_archive)
				{
					FATAL("malloc - %s\n",strerror(errno));
					return -1;
				}
				fd = open(*file,O_RDONLY);
				if(fd<0)
				{
					ERROR("cannot open file \"%s\" - %s\n",*file,strerror(errno));
					free(cpio_archive);
					return -1;
				}
				if((raeded = read(fd,cpio_archive,len)) != len)
				{
					free(cpio_archive);
					close(fd);
					if(readed < 0)
						ERROR("cannot read \"%s\" - %s\n",*file,strerror(errno));
					else
						ERROR("size mismatch: readed %ul bytes but %ul expected\n",readed,len);
					return -1;
				}
				close(fd);
			}
			umount(mountpoint);
			if(mount("tmpfs",mountpoint,"tmpfs",MS_RELATIME,""))
			{
				ERROR("cannot mount tmpfs - %s\n",strerror(errno));
				free(cpio_archive);
				return -1;
			}
			if(cpio_copyin(cpio_archive,len,mountpoint))
			{
				umount(mountpoint);
				free(cpio_archive);
				return -1;
			}
			free(cpio_archive);
			len = strlen(mountpoint);
			free(*file);
			*file = malloc((len+1)*sizeof(char));
			if(!*file)
			{
				FATAL("malloc - %s\n", strerror(errno));
				umount(mountpoint);
				return -1;
			}
			strncpy(*file,mountpoint,len);
			(*file)[len] = '\0';
		}
	}
	return 0;
}
