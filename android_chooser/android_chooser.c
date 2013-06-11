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
 * TODO! - hacking is a funny thing but many times you have to restart from 0!
 * 
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

FILE * logfile;

#include "utils.h"
#include "mountpoints.h"
#include "android_chooser.h"

/* make /dev from /sys */
void mdev(void)
{
	pid_t pid;
	if(!(pid = fork()))
	{
		char *mdev_argv[] = MDEV_ARGS;
		chdir(WORKING_DIR);
		chroot(WORKING_DIR);
		execv(BUSYBOX,mdev_argv);
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

	if((fd = open("proc/cmdline",O_RDONLY)) < 0)
		return -1;
	if((read(fd, dest, COMMAND_LINE_SIZE*(sizeof(char)))) < 0)
	{
		close(fd); // mmmm..this can change errno
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
  char line[MAX_LINE],*pos,*android_mountpoint,*fake_file;
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
	fake_file = malloc(len+DATADIR_STRLEN+1);
	if(!fake_file)
	{
		free(android_mountpoint);
		fclose(fp);
		return -1;
	}
	strncpy(fake_file,DATADIR,DATADIR_STRLEN);
	strncpy(fake_file+DATADIR_STRLEN,pos-len,len);
	*(fake_file+len+DATADIR_STRLEN) = '\0';
	*list = add_mountpoint(*list,android_mountpoint,NULL,fake_file,NULL);
  }
  fclose(fp);
  return 0;
}

const char *find_android_fstab(void)
{
	DIR *d;
	struct dirent *de;
	static char path[MAX_LINE];
	
	if(!(d=opendir("/")))
		return NULL;
	while((de=readdir(d)))
		if(de->d_type == DT_REG && !strncmp(de->d_name,"fstab.",5))
			break;
	closedir(d);
	if(!de)
	{
		fprintf(logfile,"cannot find android_fstab\n");
		return NULL;
	}
	snprintf(path,MAX_LINE,"/%s",de->d_name);
	return path;
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
	fp = fopen(android_fstab,"r");
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
				*(current->android_blkdev+len1) = '\0';
			}
	}
	return 0;
}

//TODO: remove_not_founded()

/* parse line as "blkdev:initrd_path:fstab_path"
 * returned values are:
 *	0 if ok
 *	1 if a malloc error occourred or
 *		if a parse error occourred.
 * 	if an error occour errno it's set to the corresponding error number.
 * 
 * in this version we mounted /dev on ./dev, just remove the first '/' char.
 */
int parser(char *line,char **blkdev, char **initrd_path, char **fstab_path)
{
	register char *pos;
	register int i;

	// truncate on first space
	for(pos=line;*pos!='\0'&&*pos!=' ';pos++);
	*pos='\0';

	// skip first '/'
	for(pos=line;*pos!='\0'&&*pos=='/';pos++);
	// count arg length
	for(i=0;*pos!=':'&&*pos!='\0';pos++)
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
	strncpy(*blkdev,(pos-i),i);
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

int loop_binder(mountpoint *list)
{
	int retries,done,ret;
	mountpoint *current;
	char buffer[MAX_LINE];
	
	//write loop devices names
	//HACK: use retries as counter
	for(retries=0,current=list;current;current=current->next,retries++)
		if((current->fake_blkdev = malloc(7*sizeof(char))) == NULL)
			return -1;
		else
			snprintf(current->fake_blkdev,7,"loop%d",retries);
	//associate files to loop devices
	for(done=retries=0;retries<3&&!done;retries++,sleep(1))
		for(done=1,current=list;current;current=current->next)
		{
			snprintf(buffer,MAX_LINE,"dev/%s",current->fake_blkdev);
			if((ret = set_loop(buffer,current->fake_file,&(current->fake_blkdev_fd))) == 2)
				done = 0;
			else if(ret == 1) // fatal error, remove this mountpoint
				; // TODO
		}
	return 0;
}

int copy(char *source,char*dest)
{
	int sfd,dfd,cnt;
	char buffer[MAX_LINE];
	
	if((sfd=open(source,O_RDONLY))==-1)
	{
		fprintf(logfile,"unable to open \"%s\"\n",source);
		return -1;
	}
	if((dfd=open(dest,O_WRONLY|O_TRUNC|O_CREAT))==-1)
	{
		cnt=errno;
		fprintf(logfile,"unable to open \"%s\"\n",dest);
		close(sfd);
		errno=cnt;
		return -1;
	}
	while((cnt = read(sfd,buffer,MAX_LINE))> 0)
		write(dfd,buffer,cnt);
	close(sfd);
	close(dfd);
	if(cnt<0) // read error
		return -1;
	return 0;
}

int change_android_fstab(mountpoint *list,const char * android_fstab)
{
	FILE *fp;
	char buffer[MAX_LINE],*start;
	int tmp_fd,len,len2,len3;
	mountpoint *current;
	
	if((tmp_fd = open(TMP_FSTAB,O_WRONLY|O_CREAT|O_TRUNC)) <0 )
		return -1;
	if(!(fp = fopen(android_fstab,"r")))
		return -1;
	while(fgets(buffer,MAX_LINE,fp))
	{
		len = strlen(buffer);
		for(current=list;current;current=current->next)
			if((start = strstr(buffer,current->android_blkdev)))
			{
				// measure how much is long the android_blkdev
				len2=strlen(current->android_blkdev);
				// subtract the new length
				len2-=snprintf(NULL,0,"/dev/block/%s",current->fake_blkdev);
				// sobstitute to the older with a padd
				if(len2>=0)
				{
					len3=strlen(current->fake_blkdev);
					strncpy(start,"/dev/block/",11);
					strncpy(start+11,current->fake_blkdev,len3);
					for(start=start+11+len3;*start!='\0'&&*start!=' '&&*start!='\t';start++)
						*start=' ';
				}
				else
					fprintf(logfile,"android_blkdev is too short...TODO!\n");
#ifdef DEBUG
				fprintf(logfile,"changed a fstab line:\n%s",buffer);
#endif
				break;
			}
		write(tmp_fd,buffer,len);
	}
	close(tmp_fd);
	fclose(fp);
	copy(TMP_FSTAB,(char *)android_fstab);
	unlink(TMP_FSTAB);
	return 0;
}

int main(int argc, char **argv)
{
	char *line,          	// where we place the readed line
            *start,			// where our args start
            *initrd_path,   // path to android initrd
			*fstab_path,	// path to our fstab file
            *blkdev,        // block device to mount DATADIR
			*init_argv[] = { "/init", NULL}; // init argv
	time_t timeout;
	const char *android_fstab;
	//int i; // general purpose integer
	//pid_t udev_pid;			// the pid of android_udev process
	mountpoint *list = NULL;

	android_fstab = line = start = initrd_path = fstab_path = blkdev = NULL;
	//i=0;
	if(chdir(WORKING_DIR) || (logfile = fopen(LOG,"w")) == NULL)
	{
		exit(EXIT_FAILURE);
	}
	// mount /proc
	if(mount("proc", "proc", "proc", MS_RELATIME, ""))
	{
		EXIT_ERRNO("unable to mount /proc");
	}
	// alloc line
	if((line = malloc(COMMAND_LINE_SIZE*sizeof(char))) == NULL)
	{
		umount("proc");
		EXIT_ERRNO("malloc");
	}
	// read cmdline
	if(read_cmdline(line))
	{
		umount("proc");
		free(line);
		EXIT_ERRNO("unable to read /proc/cmdline");
	}
	umount("proc");
	if (!(start=strstr(line,CMDLINE_OPTION)))
	{
		free(line);
		EXIT_ERROR("unable to find \"%s\" in \"%s\"\n",CMDLINE_OPTION,line);
	}
	start+=CMDLINE_OPTION_LEN;
	if(parser(start,&blkdev,&initrd_path,&fstab_path))
	{
		free(line);
		EXIT_ERRNO("cmdline parsing failed");
	}
	free(line);
	if(mount("sysfs","sys","sysfs",MS_RELATIME,""))
	{
		free(blkdev);
		free(initrd_path);
		free(fstab_path);
		EXIT_ERRNO("unable to mount /sys");
	}
	mdev();
	// make sure this was made
	if(access(blkdev, R_OK))
	{
		timeout = time(NULL) + TIMEOUT;
		do
		{
			usleep(100000); // 100 ms
			mdev();
		}
		while(access(blkdev, R_OK) && time(NULL) < timeout);
	}
	//mount blkdev on DATADIR
	if(mount(blkdev,DATADIR,"ext4",0,""))
	{
		EXIT_ERRNO("unable to mount \"/%s\" on %s",blkdev,DATADIR);
		free(blkdev);
		free(initrd_path);
		free(fstab_path);
	}
	free(blkdev);
#ifdef PERSISTENT_LOG
	fclose(logfile);
	logfile=fopen(PERSISTENT_LOG,"w");
#endif
	// remove init symlink
	if(unlink("/init"))
	{
		free(initrd_path);
		free(fstab_path);
		EXIT_ERRNO("cannot remove /init symlink");
	}
	//extract android initrd over /
	if(initrd_extract(initrd_path,"/"))
	{
		EXIT_ERRNO("initrd_extract \"%s\"",initrd_path);
		free(initrd_path);
		free(fstab_path);
	}
	free(initrd_path);
	//parse fstab
	if(fstab_parser(fstab_path,&list)) // only malloc troubles here ?
	{
		free(fstab_path);
		EXIT_ERRNO("fstab_parser");
	}
	free(fstab_path);
#ifdef DEBUG
	mountpoint *tmp;
	fprintf(logfile,"DEBUG: after fstab_prser\nampoint,file\n");
	for(tmp=list;tmp;tmp=tmp->next)
		fprintf(logfile,"%s,%s\n",tmp->android_mountpoint,tmp->fake_file);
#endif
	android_fstab = find_android_fstab();
	if(find_android_blockdev(list,android_fstab))
		EXIT_ERRNO("find_android_blockdev");
#ifdef DEBUG
	fprintf(logfile,"DEBUG: after find_android_blockdev\nampoint,ablkdev\n");
	for(tmp=list;tmp;tmp=tmp->next)
		fprintf(logfile,"%s,%s\n",tmp->android_mountpoint,tmp->android_blkdev);
#endif
	if(loop_binder(list))
		EXIT_ERRNO("loop_binder");
#ifdef DEBUG
	fprintf(logfile,"DEBUG: after loop_binder\nablkdev,blkdev,fd\n");
	for(tmp=list;tmp;tmp=tmp->next)
		fprintf(logfile,"%s,%s,%d\n",tmp->android_blkdev,tmp->fake_blkdev,tmp->fake_blkdev_fd);
#endif	
	if(change_android_fstab(list,android_fstab))
		EXIT_ERRNO("change_android_fstab");
	fclose(logfile);
	free_list(list);
	chdir("/");
	execv(init_argv[0],init_argv);
	exit(EXIT_FAILURE);
}
