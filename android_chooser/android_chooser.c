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
#include <ctype.h>

FILE * logfile;

#include "mimetypes.h"
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
 * source dest
 * return 0 on success.
 * NOTE: i don't like spaces in names...
 * NOTE: we don't check for source and dest existence here,
 * 		we must be able to separate these things, in order
 * 		to wait for other async stuff before checking.
 */
int fstab_parser(char *file, mountpoint **list )
{
  char line[MAX_LINE],*pos,*start,*source,*dest;
  FILE *fp;
  int len,line_no;
  
  if(!(fp = fopen(file,"r")))
    return -1;
  line_no = 0;
  while(fgets(line,MAX_LINE,fp))
  {
	line_no++;
	fgets_fix(line);
	//skip spaces
	for(start=line;*start!='\0'&&isspace(*start);start++);
	if(*start=='\0'||*start=='#') // skip comments and empty lines
		continue;
    for(len=0,pos=start;*pos!='\0'&&!isspace(*pos);pos++,len++);
    if(!len)
    {
		fprintf(logfile,"no source at line #%d\n",line_no);
		fclose(fp);
		return -1;
	}
    source = malloc(len+1);
	if(!source)
	{
		fclose(fp);
		return -1;
	}
    strncpy(source,start,len);
	*(source+len) = '\0';
	for(;*pos!='\0'&&isspace(*pos);pos++);
    for(len=0;*pos!='\0'&&!isspace(*pos);pos++,len++);
	if(!len)
    {
		fprintf(logfile,"no mountpoint at line #%d\n",line_no);
		free(source);
		fclose(fp);
		return -1;
	}
	dest = malloc(len+1);
	if(!dest)
	{
		free(source);
		fclose(fp);
		return -1;
	}
	strncpy(dest,pos-len,len);
	*(dest+len) = '\0';
	*list = add_mountpoint(*list,source,dest);
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

source_type find_file_type(char *file, struct stat info)
{
		//char magic[MAX_MAGIC_LEN];
		//int fd,len;
		
		errno = EINVAL;
		if(S_ISDIR(info.st_mode))
				return DIRECTORY;
		if(S_ISBLK(info.st_mode))
				return BLKDEV;
		if(!S_ISREG(info.st_mode))
				return NONE;
		/* we have a regular file, it can be one of the following:
		 * - filesystem image file
		 * - compressed cpio archive
		 * - cpio archive
		 */
		/*
		if((fd = open(file,O_RDONLY)) < 0)
		 		return NONE;
		if(read(fd,magic,MAX_MAGIC_LEN) != MAX_MAGIC_LEN)
		{
				close(fd);
				return NONE;
		}
		close(fd);
		len = strlen(GZIP_MAGIC);
		if(!strncmp(magic,GZIP_MAGIC,len))
		{
				if(read_first_bytes_of_archive(file,magic,MAX_MAGIC_LEN))
				{
						// TODO: gzip have a custom error handler, so errno doesn't contains the real error in many cases
						fprintf(logfile,"read_first_bytes_of_archive \"%s\" - %s",file,strerror(errno));
						return NONE;
				}
				len = strlen(CPIO_MAGIC);
				if(!strncmp(magic,CPIO_MAGIC,len) || !strncmp(magic,CPIO_MAGIC_SVR4,len) || !strncmp(magic,CPIO_MAGIC_CRC,len))
						return CPIO_ARCHIVE;
				// FIXME: we can support more compressed files types...but there is many troubles in mounting compressed fs images
				return NONE;
		}
		else if (!strncmp(magic,CPIO_MAGIC,(len = strlen(CPIO_MAGIC))) ||
								!strncmp(magic,CPIO_MAGIC_SVR4,len) ||
								!strncmp(magic,CPIO_MAGIC_CRC,len) )
		{
				return CPIO_ARCHIVE;
		}
		*/
		return IMAGE_FILE;
}

/** fill item struct with infos about source
 * @item:	the item to fill with infos
 * @source: the probed source
 * return  1 if file does not exist
 * return  0 if all it's ok
 * return -1 if something goes wrong
 */
int get_source_infos(mountpoint *item, char *source)
{
		struct stat info;
		
		if(stat(source,&info))
		{
			// probably it's something created by the android boot process
			item->s_type = DEFAULT_TYPE;
			item->options = DEFAULT_OPTS;
			return 1;
		}
		item->s_type = find_file_type(source,info);
		if(item->s_type == NONE)
		{
			fprintf(logfile,"find_file_type \"%s\" - %s\n",source,strerror(errno));
			return -1;
		}
		else if(item->s_type == IMAGE_FILE || item->s_type == BLKDEV)
		{
			item->filesystem = find_filesystem(source);
			if(!item->filesystem)
			{
				fprintf(logfile,"find_filesystem \"%s\" - %s\n",source,strerror(errno));
				return -1;
			}
		}
		switch (item->s_type) // set options
		{
			case DIRECTORY:
				item->options = BIND;
				break;
			case BLKDEV:
				item->options = WAIT;
				break;
			default:
				item->options = NONE;
		}
		return 0;
}
// TODO: check iif there is mounpoints with the same blkdev,
//			if yes we have to bind them.
int get_mountpoint_infos(mountpoint *item)
{
	char buffer[MAX_LINE],*pos;
	int len;
	
	// prepare path
	for(pos=item->blkdev;*pos!='\0'&&*pos=='/';pos++); // skip leading '/'
	snprintf(buffer,MAX_LINE,"%s%s",DATADIR,pos);
	// test if file it's an absolute path
	if((len = get_source_infos(item,item->blkdev)) < 0)
		return -1;
	else if(!len)
		;
	// test if file it's on user data blkdev
	else if((len = get_source_infos(item,buffer)) < 0)
		return -1;
	else if(!len)
	{
		free(item->blkdev);
		len = strlen(buffer) +1; // +1 for copy '\0' too
		item->blkdev = malloc(len);
		if(!item->blkdev)
			return -1;
		memcpy(item->blkdev,buffer,len);
	}
	// give an hint to who don't RTFM
	else if(strstr(item->blkdev,"/dev/block"))
	{
			len=errno;
			fprintf(logfile,"we use standard /dev schemas, use /dev/mmcblk0p1 instead of /dev/block/mmcblk0p1!\n");
			errno=len;
			return -1;
	}
	return 0;
}

mountpoint *check_list(mountpoint *list)
{
	mountpoint *current,*old;
	
	for(current=list;current;)
		if(get_mountpoint_infos(current))
		{
			old = current;
			current = current->next;
			list = del_mountpoint(list,old);
		}
		else
			current = current->next;
	return list;
}

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

mountpoint *loop_binder(mountpoint *list)
{
	int retries,done,ret,loop_no;
	mountpoint *current,*old;
	char buffer[MAX_LINE];
	
	for(retries=done=0;retries<3&&!done;retries++)
		for(loop_no=0,done=1,current=list;current;)
			if(current->s_type == IMAGE_FILE)
			{
				snprintf(buffer,MAX_LINE,"dev/loop%d",loop_no);
				if((ret = set_loop(buffer,current->blkdev,&(current->blkdev_fd))) == 2)
					done = 0;
				else if(ret == 1) // fatal error, remove this mountpoint
				{
					fprintf(logfile,"set_loop \"%s\" on \"%s\" - %s\n",current->blkdev,buffer,strerror(errno));
					fprintf(logfile,"removing \"%s\" mountpoint\n",current->mountpoint);
					old = current;
					current=current->next;
					list = del_mountpoint(list,old);
				}
				else // we did it, now file it's associated to loop device
				{
					free(current->blkdev); // replace the file path
					ret = snprintf(buffer,MAX_LINE,"/dev/block/loop%d",loop_no); // with the loop device one
					current->blkdev = malloc(ret+1);
					memcpy(current->blkdev,buffer,ret+1);
					current->s_type = BLKDEV; // now it's a blockdevice
					current=current->next;
				}
				loop_no++;
				// maybe we can encounter troubles if we delete a mountpoint in the middle...
				// we have to check the set_loop function in details.
			}
			else
				current=current->next;
	return list;
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
	char buffer[MAX_LINE],*start,*pos,*writeme;
	int tmp_fd,len,len2,offset;
	mountpoint *current;
	struct fstab_record
	{
		char 	blkdev[MAX_LINE],
				mountpoint[MAX_LINE],
				type[MAX_LINE],
				mnt_flags[MAX_LINE],
				fs_mgr_flags[MAX_LINE];
	} record;
	
	if((tmp_fd = open(TMP_FSTAB,O_WRONLY|O_CREAT|O_TRUNC)) <0 )
		return -1;
	if(!(fp = fopen(android_fstab,"r")))
		return -1;
	offset=sizeof(record.blkdev); // or MAX_LINE*sizeof(char)
	while(fgets(buffer,MAX_LINE,fp))
	{
		len=strlen(buffer);
		for(start=buffer;*start!='\0'&&isspace(*start);start++);
		if(*start!='\0'&&*start!='#')
		{
			// HACK: use a pointer for loop on struct.
			for(writeme=record.blkdev;writeme<=record.fs_mgr_flags;writeme+=offset)
			{
				// run to the next space
				for(pos=start;*pos!='\0'&&!isspace(*pos);pos++);
				// copy it
				len2=pos-start;
				memcpy(writeme,start,len2);
				*(writeme+len2)='\0';
				// run to the next not-space
				for(start=pos;*start!='\0'&&isspace(*start);start++);
			}
			// use only the first result
			len2=strlen(record.mountpoint);
			for(current=list;current&&strncmp(record.mountpoint,current->mountpoint,len2);current=current->next);
			if(current)
			{
				// overwrite fs type
				if(current->filesystem)
					strncpy(record.type,current->filesystem,MAX_LINE);
				else
					strncpy(record.type,"none",5);
				// overwrite blockdevice
				strncpy(record.blkdev,current->blkdev,MAX_LINE);
				// modify mnt_flags
				if(current->options & BIND)
				{
					if(record.mnt_flags[0] != '\0')
					{
						len2=snprintf(buffer,MAX_LINE,"%s,%s",record.mnt_flags,options_str[BIND]);
						memcpy(record.mnt_flags,buffer,len2+1);
					}
					else
						strncpy(record.mnt_flags,options_str[BIND],MAX_LINE);
				}
				// modify fs_mgr_flags
				if(current->options & WAIT)
				{
					if(record.fs_mgr_flags[0] != '\0')
					{
						len2=snprintf(buffer,MAX_LINE,"%s,%s",record.fs_mgr_flags,options_str[WAIT]);
						memcpy(record.fs_mgr_flags,buffer,len2+1);
					}
					else
						strncpy(record.fs_mgr_flags,options_str[WAIT],MAX_LINE);
				}
				len = snprintf(buffer,MAX_LINE,"%s\t%s\t%s\t%s\t%s\n",
						record.blkdev,record.mountpoint,
						record.type,record.mnt_flags,record.fs_mgr_flags);
				current->processed=1;
#ifdef DEBUG
				fprintf(logfile,"changed a fstab line:\n%s",buffer);
#endif
			}
		}
		write(tmp_fd,buffer,len);
	}
	// write not founded mountpoints
	// TODO: remove the leading '#'
	for(current=list;current;current=current->next)
		if(!current->processed)
		{
			len = snprintf(buffer,MAX_LINE,"#%s\t%s\t%s\t\n",
					current->blkdev,current->mountpoint,current->filesystem);
			write(tmp_fd,buffer,len);
		}
	
	close(tmp_fd);
	fclose(fp);
	copy(TMP_FSTAB,(char *)android_fstab);
	unlink(TMP_FSTAB);
#ifdef FSTAB_PERSISTENT
	copy((char *)android_fstab,FSTAB_PERSISTENT);
#endif
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
	// remove /bin symlink
	if(unlink("/bin"))
	{
		free(fstab_path);
		EXIT_ERRNO("cannot remove /bin symlink");
	}
	//parse fstab
	if(fstab_parser(fstab_path,&list))
	{
		free(fstab_path);
		EXIT_ERRNO("fstab_parser");
	}
	free(fstab_path);
	android_fstab = find_android_fstab();
	if((list = check_list(list)) == NULL)
		EXIT_ERRNO("check_list");
	if((list = loop_binder(list)) == NULL)
		EXIT_ERRNO("loop_binder");
#ifdef DEBUG
	mountpoint *tmp;
	fprintf(logfile,"DEBUG - entries:\n");
	for(tmp=list;tmp;tmp=tmp->next)
		fprintf(logfile,"%s, %s, %s, %d, %d, %d, %d\n",
				tmp->mountpoint,tmp->blkdev,tmp->filesystem,
				(int)tmp->options,tmp->processed,tmp->blkdev_fd,(int)tmp->s_type);
#endif
	if(change_android_fstab(list,android_fstab))
		EXIT_ERRNO("change_android_fstab");
	fclose(logfile);
	free_list(list);
	chdir("/");
	execv(init_argv[0],init_argv);
	exit(EXIT_FAILURE);
}
