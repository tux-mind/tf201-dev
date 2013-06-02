/*
 * android_chooser - v1 - choose the android system.
 * Copyright (C) massimo dragano <massimo.dragano@gmail.com>
 *               Smasher816 <smasher816@gmail.com>
 * android_chooser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * android_chooser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * android_chooser works as follow:
 *
 * 1) read configuration from kernel commandline
 * 2) extract android initrd over NEWROOT
 * 3) run android udev chrooted into NEWROOT
 * 4) read fstab file
 * 5) find what android blockdevices handle our mountpoints
 * 6) associate fs files to loop devices
 * 7) wait that android blockdevices has been found by udev
 * 8) replace the android blockdevices with symlinks to loop devices
 * 9) chroot into NEWROOT and start the android init process
 * 
 * TODO: android init will start udev again, check if it overwrite our hack.
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

#include "mountpoints.h"
#include "android_chooser.h"

FILE *log;

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
 * return 0 on success, -1 on error
 * WARN: dest MUST be at least COMMAND_LINE_SIZE long
 */
int read_cmdline(char *dest)
{
	int fd;

	memset(dest,'\0',COMMAND_LINE_SIZE);

	if((fd = open("/proc/cmdline",O_RDONLY)) < 0)
		return -1;
	if((read(fd, dest, COMMAND_LINE_SIZE*(sizeof(char)))) < 0)
	{
		close(fd);
		return -1;
	}
	close(fd);
	// cmdline is stored with a final '\n', we don't like this
	fgets_fix(dest);
	return 0;
}

/* parse file as follow:
 * /android/mountpoint /path/to/image/file
 * return 0 on success.
 * NOTE: i don't like spaces in names...
 */
int fstab_parser(char *file, mountpoint **list )
{
  char line[MAX_LINE],*pos,*pos2,*android_mountpoint,*fake_file;
  FILE *fp;
  int len;
  
  if(!(fp = fopen(file,"r")))
    return -1;
  while(fgets(line,MAX_LINE,fp))
  {
	fgets_fix(line);
    for(len=0,pos=line;*pos!='\0'&&*pos!=' '&&*pos!='\t';pos++,len++);
    android_mountpoint = malloc(len+1);
	if(!android_mountpoint)
	{
		fclose(fp);
		return -1;
	}
    strncpy(android_mountpoint,line,len);
	*(android_mountpoint+len) = '\0';
	// skip spaces and trailing '/'
	for(;*pos!='\0'&&(*pos==' '||*pos=='\t'||*pos=='/');pos++);
    for(len=0;*pos!='\0'&&*pos!=' '&&*pos!='\t';pos++,len++);
	fake_file = malloc(len+1);
	if(!fake_file)
	{
		free(android_mountpoint);
		fclose(fp);
		return -1;
	}
	strncpy(fake_file,pos-len,len);
	*(fake_file+len) = '\0';
	*list = add_mountpoint(*list,android_mountpoint,NULL,fake_file,NULL);
  }
  fclose(fp);
  return 0;
}

const char *find_android_fstab(void)
{
	DIR *d;
	struct dirent *de;
	if(!(d=opendir(NEWROOT)))
		return NULL;
	while((de=readdir(d)))
		if(de->d_type == DT_REG && !strncmp(de->d_name,"fstab.",5))
			break;
	closedir(d);
	if(!de)
		return NULL;
	return de->d_name; //it's statically allocated...
}

int find_android_blockdev(mountpoint *list,const char *android_fstab)
{
	FILE *fp;
	char line[MAX_LINE],*pos;
	int len1,len2;
	mountpoint *current;
	
	if(!list)
		return 0;
	else if(!android_fstab)
		return -1;
	fp = fopen(android_fstab);
	if(!fp)
		return -1;
	while(fgets(line,MAX_LINE,fp))
	{
		fgets_fix(line);
		for(len1=0,pos=line;*pos!='\0'&&*pos!='\t'&&*pos!=' ';pos++,len1++);
		//skip spaces
		for(;*pos!='\0'&&(*pos==' '||*pos=='\t');pos++);
		for(len2=0;*pos!='\0'&&*pos!='\t'&&*pos!=' ';pos++,len2++);
		*pos='\0'; // truncate for strncmp
		//search in list
		for(current=list;current;current=current->next)
			if(!strncmp((pos-len2),current->android_mountpoint,len2+1))
			{
				current->android_blkdev = malloc(len1+1);
				if(!current->android_blkdev)
					return -1;
				strncpy(current->android_blkdev,line,len1);
			}
	}
}


/* parse line as "blkdev:initrd_path:fstab_path"
 * returned values are:
 *	0 if ok
 *	1 if a malloc error occourred or
 *		if a parse error occourred.
 * 	if an error occour errno it's set to the corresponding error number.
 */
int parser(char *line,char **blkdev, char **initrd_path, char **fstab_path)
{
	register char *pos;
	register int i;
	int j;

	// truncate on first space
	for(pos=line;*pos!='\0'&&*pos!=' ';pos++);
	*pos='\0';

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
	if(!i)
	{
		free(*blkdev);
		errno = EINVAL;
		return 1;
	}
	*initrd_path = malloc((i+DATADIR_STRLEN+1)*sizeof(char));
	if(!*initrd_path)
	{
		free(*blkdev);
		return 1;
	}
	// copy DATADIR to initrd_path
	strncpy(*initrd_path,DATADIR,DATADIR_STRLEN);
	// append user root_directory to DATADIR
	strncpy(*initrd_path+DATADIR_STRLEN,pos - i,i);
	*(*initrd_path + DATADIR_STRLEN+i) = '\0';
	if(*pos==':')
		pos++;
	// skip trailing '/'
	if(*pos=='/')
		pos++;
	for(i=0;*pos!=':'&&*pos!='\0';pos++)
		i++;
	if(!i)
	{
		free(*blkdev);
		free(*initrd_path);
		errno = EINVAL;
		return 1;
	}
	*fstab_path = malloc((i+DATADIR_STRLEN+1)*sizeof(char));
	if(!*fstab_path)
	{
		free(*blkdev);
		free(*initrd_path);
		return 1;
	}
	// copy DATADIR to fstab_path
	strncpy(*fstab_path,DATADIR,DATADIR_STRLEN);
	// append user root_directory to DATADIR
	strncpy(*fstab_path+DATADIR_STRLEN,pos - i,i);
	*(*fstab_path + DATADIR_STRLEN+i) = '\0';
	return 0; // all ok
}

int main(int argc, char **argv, char **envp)
{
	char *line,          // where we place the readed line
             *start,         // where our args start
             *root,          // directory to chroot
             *blkdev;        // block device to mount on newroot
	int i,mounted_twice; // general purpose integer
	mountpoint *list = NULL;

	line = blkdev = root = NULL;
	i=mounted_twice=0;
			
	if((log = fopen(LOG,"w")) == NULL)
	{
		fatal(argv,envp);
		exit(EXIT_FAILURE);
	}
	// mount /proc
	if(mount("proc", "/proc", "proc", MS_RELATIME, ""))
	{
		EXIT_ERROR("unable to mount /proc");
	}
	// alloc line
	if((line = malloc(COMMAND_LINE_SIZE*sizeof(char))) == NULL)
	{
		umount("/proc");
		EXIT_ERROR("malloc");
	}
	// read cmdline
	if(read_cmdline(line))
	{
		umount("/proc");
		free(line);
		EXIT_ERROR("unable to read /proc/cmdline");
	}
	umount("/proc");
	if (!(start=strstr(line,CMDLINE_OPTION)))
	{
		fprintf(log,"unable to find \"%s\" in \"%s\"\n",CMDLINE_OPTION,line);
		free(line);
		EXIT_SILENT;
	}
	start+=CMDLINE_OPTION_LEN;
	if(parser(start,&blkdev,&root,&new_argv))
	{
		free(line);
		EXIT_ERROR("parsing failed");
	}
	free(line);
	if(mount("sysfs","/sys","sysfs",MS_RELATIME,""))
	{
		EXIT_ERROR("unable to mount /sys");
	}
	mdev(envp);
	// make sure this was made
	for(i=1;access(blkdev, R_OK) && i < TIMEOUT;i++)
	{
		sleep(1);
		mdev(envp);
	}
	umount("/sys");
	//mount blkdev on NEWROOT
	if(mount(blkdev,NEWROOT,"ext4",0,""))
	{
		fprintf(log,"unable to mount \"%s\" on %s - %s\n",blkdev,NEWROOT,strerror(errno));
		free(blkdev);
		free(root);
		for(i=0;new_argv[i];i++)
			free(new_argv[i]);
		EXIT_SILENT;
	}
	free(blkdev);
	blkdev = root; // keep a track of the old pointer
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
	fatal(argv,envp);
	exit(EXIT_FAILURE);
}
