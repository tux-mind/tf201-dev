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
 * root_choooser works as follow:
 *
 * 1) mount proc on /proc
 * 2) read the contents of /proc/cmdline
 * 3) search for "newroot="
 * 4) parse as "block_device:root_directory:init_path,init_args"
 * 5) mount block_device on /newroot
 * 6) if /newroot/root_directory is a ext img mount it on /newroot
 * 7) if /newroot/root_directory is an initramfs mount it on /newroot
 * 8) chroot /newroot/root_directory
 * 9) execve init_script
 *
 * ** NOTE **
 * if something goes wrong or the first char
 * of the readed line is '#', goto point 8
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

#include "root_chooser6.h"
//fatal error occourred, boot up android
void fatal(char **argv,char **envp)
{
	//TODO: maybe we must make some error checking also there...
	//lock android boot if wanna only adb
#ifndef ADB
	chdir(NEWROOT);
#endif
	chroot(NEWROOT);
	execve("/init",argv,envp);
}

/* make /dev from /sys */
void mdev(char **envp)
{
	pid_t pid;
	if(!(pid = fork()))
	{
		char *mdev_argv[] = MDEV_ARGS;
		execve(BUSYBOX,mdev_argv,envp);
	}
	waitpid(pid,NULL,0);
}

/* substitute '\n' with '\0' */
void fgets_fix(char *string)
{
	char *pos;
	for(pos=string;*pos!='\n'&&*pos!='\0';pos++);
	*pos='\0';
}

/* read the current cmdline from proc
 * return the size of the readed command line.
 * if an error occours 0 is returned.
 * WARN: dest MUST be at least COMMAND_LINE_SIZE long
 */
int read_our_cmdline(char *dest)
{
	int fd,len;

	memset(dest,'\0',COMMAND_LINE_SIZE);

	if((fd = open("/proc/cmdline",O_RDONLY)) < 0)
		return -1;
	if((len = read(fd, dest, COMMAND_LINE_SIZE*(sizeof(char)))) < 0)
	{
		close(fd);
		return -1;
	}
	close(fd);
	for(fd=0;fd<len;fd++)
		if(dest[fd]=='\n')
		{
			dest[fd]='\0';
			len = fd;
			break;
		}
	return len;
}

/* parse line as "blkdev:root_directory:init_path,init_arg1,init_arg2..."
 * returned values are:
 *	0 if ok
 *	1 if a malloc error occourred or
 *		if a parse error occourred.
 * 	if an error occour errno it's set to the corresponding error number.
 */
int parser(char *line,char **blkdev, char **root, char ***init_args)
{
	register char *pos;
	char *args_pool[INIT_MAX_ARGS+1];
	register int i;
	int j;

	// init args_pool ( will free anything != 0 if an error occour )
	memset(args_pool,'\0',(INIT_MAX_ARGS+1)*sizeof(char*));

	// count args length
	for(i=0,pos=line;*pos!=':'&&*pos!='\0';pos++)
		i++;
	// check for arg length
	if(!i)
	{
		errno = EINVAL;
		return 1;
	}
	// allocate memory dynamically ( i love this things <3 )
	*blkdev = malloc((i+1)*sizeof(char));
	if(!*blkdev)
		return -1;
	// copy string
	strncpy(*blkdev,line,i);
	*(*blkdev+i) = '\0';
	// skip token
	if(*pos==':')
		pos++;
	// skip trailing '/'
	if(*pos=='/')
		pos++;
	for(i=0;*pos!=':'&&*pos!='\0';pos++)
		i++;
	if(!i && *(pos-1) != '/')
	{
		free(*blkdev);
		errno = EINVAL;
		return 1;
	}
	*root = malloc((i+NEWROOT_STRLEN+1)*sizeof(char));
	if(!*root)
	{
		free(*blkdev);
		return 1;
	}
	// copy NEWROOT to root
	strncpy(*root,NEWROOT,NEWROOT_STRLEN);
	// append user root_directory to NEWROOT
	strncpy(*root+NEWROOT_STRLEN,pos - i,i);
	*(*root + NEWROOT_STRLEN+i) = '\0';
	if(*pos==':')
		pos++;
	// count how many args we need while store them
	for(j=0;j<INIT_MAX_ARGS && *pos != '\0';)
	{
		while(*pos==',')
			pos++;
		for(i=0;*pos!='\0'&&*pos!=',';pos++)
			i++;
		if(i) // this will fail if line terminates with space.
		{
			args_pool[j] = malloc((i+1)*sizeof(char));
			if(args_pool[j])
			{
				strncpy(args_pool[j],pos - i,i);
				args_pool[j][i]='\0';
				j++; // increase args counter only if we have created one.
			}
			else // it's useless going on....exit now!
			{
				//free() don't change errno if called with a valid pointer.
				free(*blkdev);
				free(*root);
				// free any allocated arg
				for(j=0;j<INIT_MAX_ARGS+1;j++)
					if(args_pool[j])
						free(args_pool[j]);
				return 1;
			}
		}
	}
	if(!j)
	{
		free(*blkdev);
		free(*root);
		// free any allocated arg
		for(j=0;j<INIT_MAX_ARGS+1;j++)
			if(args_pool[j])
				free(args_pool[j]);
		errno=EINVAL;
		return 1;
	}

	*init_args = malloc((j+1)*sizeof(char*));
	if(!*init_args)
	{
		// yeah, i'm really paranoid about error checking ;)
		free(*blkdev);
		free(*root);
		for(j=0;j<INIT_MAX_ARGS+1;j++)
			if(args_pool[j])
				free(args_pool[j]);
		return 1;
	}
	// copy args from args_pool to init_args
	for(i=0;i<j;i++)
		*(*init_args+i) = args_pool[i];
	*(*init_args+i) = NULL;
	return 0; // all ok
}

int main(int argc, char **argv, char **envp)
{
	FILE *log;
	char 	*line, // where we place the readed line
				*start, // where our args start
				*root, // directory to chroot
				*blkdev, // block device to mount on newroot
				**new_argv; // init args
	int i,mounted_twice; // general purpose integer

	line = blkdev = root = NULL;
	new_argv = NULL;
	i=mounted_twice=0;

	if((log = fopen(LOG,"w")) != NULL)
	{
		// mount /proc
		if(!mount("proc","/proc","proc",MS_RELATIME,""))
		{
			//alloc line
			if((line = malloc(COMMAND_LINE_SIZE*sizeof(char))))
			{
				// read cmdline
				if(!read_our_cmdline(line))
				{
					umount("/proc");
					if((start=strstr(line,CMDLINE_OPTION)))
					{
						start+=CMDLINE_OPTION_LEN;
						if(!parser(start,&blkdev,&root,&new_argv))
						{
							free(line);
							if(!mount("sysfs","/sys","sysfs",MS_RELATIME,""))
							{
								mdev(envp);
								if(access(blkdev,R_OK))
								{
									sleep(1);
									mdev(envp);
									for(i=1;access(blkdev,R_OK) && i < TIMEOUT;i++)
									{
										sleep(1);
										mdev(envp);
									}
								}
								umount("/sys");
								//mount blkdev on NEWROOT
								if(!mount(blkdev,NEWROOT,"ext4",0,""))
								{
									free(blkdev);
									blkdev = root; // kepp a track of the old pointer
									//if root is an ext image mount it on NEWROOT
									//if root is an initrd(.gz)? file extract it into NEWROOT
									if(!try_loop_mount(&root,NEWROOT) && !try_initrd_mount(&root,NEWROOT))
									{
										if(blkdev != root) // NEWROOT has been mounted again
											mounted_twice=1;
										//check for init existence
										i=strlen(root) + strlen(new_argv[0]);
										if((line=malloc((i+1)*sizeof(char))))
										{
											strncpy(line,root,i);
											strncat(line,new_argv[0],i);
											line[i]='\0';
											if(!access(line,R_OK|X_OK))
											{
												if(!chdir(root) && !chroot(root))
												{
													free(root);
													fclose(log);
													execve(new_argv[0],new_argv,envp);
												}
												else
												{
													fprintf(log,"cannot chroot/chdir to \"%s\" - %s\n",root,strerror(errno));
													free(root);
													for(i=0;new_argv[i];i++)
														free(new_argv[i]);
													fclose(log);
													umount(NEWROOT);
													if(mounted_twice)
														umount(NEWROOT);
												}
											}
											else
											{
												fprintf(log,"cannot execute \"%s\" - %s\n",line,strerror(errno));
												free(line);
												free(root);
												for(i=0;new_argv[i];i++)
													free(new_argv[i]);
												fclose(log);
												umount(NEWROOT);
												if(mounted_twice)
													umount(NEWROOT);
											}
										}
										else
										{
											fprintf(log,"malloc - %s\n",strerror(errno));
											free(root);
											for(i=0;new_argv[i];i++)
												free(new_argv[i]);
											fclose(log);
											umount(NEWROOT);
											if(mounted_twice)
												umount(NEWROOT);
										}
									}
									else
									{
										if(root) // try_loop_mount reallocate root, a malloc problem can be happend
										{
											if(blkdev!=root)
												umount(NEWROOT);
											fprintf(log,"try_(loop/initrd)_mount \"%s\" on %s - %s\n",root,NEWROOT,strerror(errno));
											free(root);
										}
										else
											fprintf(log,"try_loop_mount NULL root - %s\n",strerror(errno));
										for(i=0;new_argv[i];i++)
											free(new_argv[i]);
										fclose(log);
										umount(NEWROOT);
									}
								}
								else
								{
									fprintf(log,"unable to mount \"%s\" on %s - %s\n",blkdev,NEWROOT,strerror(errno));
									free(blkdev);
									free(root);
									for(i=0;new_argv[i];i++)
										free(new_argv[i]);
									fclose(log);
								}
							}
							else
							{
								fprintf(log,"unable to mount /sys - %s\n",strerror(errno));
								fclose(log);
							}
						}
						else
						{
							fprintf(log,"parsing failed - %s\n",strerror(errno));
							free(line);
							fclose(log);
						}
					}
					else
					{
						fprintf(log,"unable to find \"%s\" in \"%s\"\n",CMDLINE_OPTION,line);
						free(line);
						fclose(log);
					}
				}
				else
				{
					fprintf(log,"unable to read /proc/cmdline - %s\n",strerror(errno));
					umount("/proc");
					free(line);
					fclose(log);
				}
			}
			else
			{
				fprintf(log,"malloc - %s\n",strerror(errno));
				umount("/proc");
				fclose(log);
			}
		}
		else
		{
			fprintf(log, "unable to mount /proc - %s\n",strerror(errno));
			fclose(log);
		}
	}

	fatal(argv,envp);
	exit(EXIT_FAILURE);
}
