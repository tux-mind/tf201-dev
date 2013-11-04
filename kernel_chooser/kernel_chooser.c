/*
 * kernel_chooser - v1 - choose the kernel to boot.
 * Copyright (C) 	massimo dragano <massimo.dragano@gmail.com>
 *								Smasher816 <smasher816@gmail.com>
 * kernel_chooser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * kernel_chooser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* kernel_choooser works as follows:
 *
 * 1) read the contents of /data/.kernel.d/
 * 2) parse as "description \n blkdev:kernel:initrd \n cmdline"
 * 3) wait 10 seconds for the user to press a key.
 *    if no key is pressed, boot the default configuration in /data/.kernel
 *    if a key is pressed, display a menu for manual selection
 * 4) kexec hardboot into the new kernel
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
#include <ctype.h>

#include "common.h"
#include "menu.h"
#include "kernel_chooser.h"

// if == 1 => someone called FATAL we have to exit
int fatal_error;

/* make /dev from /sys */
void mdev(void)
{
	pid_t pid;
	if(!(pid = fork()))
	{
		char *mdev_argv[] = MDEV_ARGS;
		execv(BUSYBOX,mdev_argv);
	}
	waitpid(pid,NULL,0);
}

/* substitute '\n' with '\0' */
char *fgets_fix(char *string)
{
	char *pos;

	if(!string)
		return NULL;
	for(pos=string;*pos!='\n'&&*pos!='\0';pos++);
	*pos='\0';
	return string;
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
	{
		FATAL("cannot open \"/proc/cmdline\" - %s\n",strerror(errno));
		return 0;
	}
	if((len = read(fd, dest, COMMAND_LINE_SIZE*(sizeof(char)))) < 0)
	{
		FATAL("cannot read \"/proc/cmdline\" -%s\n",strerror(errno));
		close(fd);
		return 0;
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

/* if cmdline is NULL or its length is 0 => use our cmdline
 * else if cmdline starts with the '+' sign => extend our cmdline with the provided one
 * else cmdline = the provided cmdline
 */
int cmdline_parser(char *line, char **cmdline)
{
	int len;
	static char our_cmdline[COMMAND_LINE_SIZE];
	static int our_cmdline_len=0;

	if(!our_cmdline_len)
	{
		our_cmdline_len = read_our_cmdline(our_cmdline);
		if(!our_cmdline_len)
			return -1;
	}
	// use the given one
	if(line != NULL && (len = strlen(line)) > 0)
	{
		// append to our_cmdline
		if(line[0] == '+')
			len += our_cmdline_len +1; // one more for the ' '
		if(len > COMMAND_LINE_SIZE)
		{
			ERROR("command line too long\n");
			WARN("the current one will be used instead\n");
			line = NULL;
		}
	}
	else
	{
		len = our_cmdline_len;
		line = NULL;
	}

	*cmdline = malloc((len+1)*sizeof(char));
	if(!cmdline)
	{
		FATAL("malloc - %s\n",strerror(errno));
		*cmdline = NULL;
		return -1;
	}
	// use our_cmdline
	if(line == NULL)
		strncpy(*cmdline,our_cmdline,len);
	// extend our commandline
	else if(line[0] == '+')
		snprintf(*cmdline,len,"%s %s",our_cmdline,line+1);
	// use the given one
	else
		strncpy(*cmdline,line,len);
	*(*cmdline +len) = '\0';
	//*(*cmdline +len+1) = '\0';
	return 0;
}

/** parse line as "blkdev:kernel:initrd"
 * set given char ** to NULL
 * on return not allocated pointers are NULL ( for optional args like initrd )
 * returned values are:
 *	0 if ok
 *	1 if an error occours
 */
int config_parser(char *line,char **blkdev, char**kernel, char **initrd)
{
	register char *pos;
	register int i;

	*blkdev=*kernel=*initrd=NULL;

	// count args length
	for(i=0,pos=line;*pos!=':'&&*pos!='\0';pos++)
		i++;
	// check arg length
	if(!i)
	{
		ERROR("missing block device\n");
		return 1;
	}
	// allocate memory dynamically ( i love this thing <3 )
	*blkdev = malloc((i+1)*sizeof(char));
	if(!*blkdev)
	{
		FATAL("malloc - %s\n",strerror(errno));
		return -1;
	}
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
		*blkdev = NULL;
		ERROR("missing kernel\n");
		return 1;
	}
	*kernel = malloc((i+NEWROOT_STRLEN+1)*sizeof(char));
	if(!*kernel)
	{
		free(*blkdev);
		*blkdev = NULL;
		FATAL("malloc - %s\n",strerror(errno));
		return 1;
	}
	strncpy(*kernel,NEWROOT,NEWROOT_STRLEN);
	strncpy(*kernel+NEWROOT_STRLEN,pos - i,i);
	*(*kernel + NEWROOT_STRLEN+i) = '\0';
	// skip token
	if(*pos==':')
		pos++;
	// skip trailing '/'
	if(*pos=='/')
		pos++;
	for(i=0;*pos!=':'&&*pos!='\0';pos++)
		i++;
	if(i)
	{
		*initrd = malloc((i+NEWROOT_STRLEN+1)*sizeof(char));
		if(!*initrd)
		{
			free(*blkdev);
			free(*kernel);
			*kernel = *blkdev = NULL;
			FATAL("malloc - %s\n",strerror(errno));
			return 1;
		}
		// append the read value to NEWROOT
		strncpy(*initrd,NEWROOT,NEWROOT_STRLEN);
		strncpy(*initrd+NEWROOT_STRLEN,pos - i,i);
		*(*initrd + NEWROOT_STRLEN+i) = '\0';
	}
	return 0; // everyting is ok
}

/** parse a file as follows:
 * ----start----
 * DESCRIPTION/NAME
 * blkdev:kernel:initrd
 * CMDLINE
 * -----end-----
 * we require a blkdev and kernel
 * others are optionals
 * check cmdline_parser for info about CMDLINE
 * @file: the parsed file
 * @fallback_name: name to use if no one has been found
 * @list: the entries list
 */
int parser(char *file, char *fallback_name, menu_entry **list)
{
	FILE *fin;
	char name_line[MAX_NAME],line[MAX_LINE],*blkdev,*kernel,*initrd,*cmdline,*name;
	int name_len;

	blkdev=kernel=initrd=cmdline=name=NULL;

	if(!(fin=fopen(file,"r")))
	{
		if(strncmp(fallback_name,DEFAULT_CONFIG_NAME,strlen(DEFAULT_CONFIG_NAME)))
			ERROR("cannot open \"%s\" - %s\n", file,strerror(errno));
		//nothing to free, exit now
		return -1;
	}

	if(!fgets(name_line,MAX_NAME,fin) || !fgets(line,MAX_LINE,fin)) // read the second line
	{
		// error
		if(!feof(fin))
		{
			ERROR("cannot read \"%s\" - %s\n",file,strerror(errno));
			fclose(fin);
			return -1;
		}
		fclose(fin);
		WARN("file \"%s\" must have at least 2 lines\n",file);
		return -1;
	}
	fgets_fix(name_line);
	fgets_fix(line);
	//check that name/description is printable ( ncurses menu will fail otherwise )
	// http://en.wikipedia.org/wiki/ASCII#ASCII_printable_characters
	//HACK: use name_len as counter, just to do 2 things in one loop ;)
	for(name_len=0;name_line[name_len]!='\0';name_len++)
	  if(name_line[name_len] <  0x20 || name_line[name_len] > 0x7E)
	  {
	    WARN("file \"%s\" have unprintable characters in name/description\n",file);
	    return -1;
	  }
	if(!name_len) // no name
	{
		WARN("file \"%s\" don't have a DESCRIPTION/NAME\n",file);
		strncpy(name_line,fallback_name,MAX_NAME);
		name_len = strlen(name_line);
		INFO("will use \"%s\" as name\n",fallback_name);
	}
	if(!(name = malloc((name_len+1)*sizeof(char))))
	{
		fclose(fin);
		FATAL("malloc - %s\n",strerror(errno));
		return -1;
	}
	strncpy(name,name_line,name_len);
	*(name+name_len)='\0';
	if (
		!config_parser(line,&blkdev,&kernel,&initrd) &&
		!cmdline_parser(fgets_fix(fgets(line,MAX_LINE,fin)),&cmdline)
	)
	{
		fclose(fin);
		*list = add_entry(*list, name, blkdev, kernel, cmdline, initrd);
		return 0;
	}

	fclose(fin);
	if(blkdev)
		free(blkdev);
	if(kernel)
		free(kernel);
	if(initrd)
		free(initrd);
	if(cmdline)
		free(cmdline);
	if(name)
		free(name);
	return -1;
}

void take_console_control(void)
{
	int i;
	close(0);
	close(1);
	close(2);
	setsid();
	if((i = open(CONSOLE,O_RDWR|O_NOCTTY)) >= 0)
	{
		(void) ioctl(i, TIOCSCTTY, 1);
		dup(i);
		dup(i);
	}
}

/** open console for the first time
 *  NOTE: we need /sys mounted
 */
int open_console(void)
{
	int i;

	mdev();
	if(access(CONSOLE,R_OK|W_OK))
	{ // no console yet... wait until timeout
		sleep(1);
		for(i=1;access(CONSOLE,R_OK|W_OK) && i < TIMEOUT_BLKDEV;i++)
		{
			sleep(1);
			mdev();
		}
		if(i==TIMEOUT_BLKDEV) // no console availbale ( user it's using an older kernel )
		{
			errno = ETIMEDOUT;
			return -1;
		}
	}
	take_console_control();
	return 0;
}

int parse_data_directory(menu_entry **list)
{
	DIR *dir;
	struct dirent *d;

	if(chdir(DATA_DIR))
	{
		FATAL("cannot chdir to \"%s\" - %s\n",DATA_DIR,strerror(errno));
		return -1;
	}
	if((dir = opendir(".")) == NULL)
	{
		ERROR("cannot open \"%s\" - %s\n",DATA_DIR,strerror(errno));
		chdir("/");
		return -1;
	}
	while((d = readdir(dir)) != NULL)
		if(d->d_type != DT_DIR)
		{
			if(parser(d->d_name,d->d_name,list))
			{
				if(fatal_error)
				{
					closedir(dir);
					chdir("/");
					return -1;
				}
				continue;
			}
		}
	closedir(dir);
	chdir("/");
	return 0;
}

int wait_for_device(char *blkdev)
{
	int i;
	if(access(blkdev,R_OK) && !mount("sysfs","/sys","sysfs",MS_RELATIME,""))
	{
		DEBUG("block device \"%s\" not found.\n",blkdev);
		INFO("waiting for device...\n");
		sleep(1);
		mdev();
		for(i=1;access(blkdev,R_OK) && i < TIMEOUT_BLKDEV;i++)
		{
			sleep(1);
			mdev();
		}
		umount("/sys");
		if(i==TIMEOUT_BLKDEV)
			return -1;
	}
	return 0;
}

void cleanup(int data_dir_to_parse, menu_entry *list)
{
	if(data_dir_to_parse)
		umount("/data");
	else
		nc_destroy_menu();
	if (list)
		free_list(list);
	nc_destroy();
}

void init_reboot(int magic)
{
	// this could use a lot more cleanup (unmount, etc)
	cleanup(0,NULL);
	reboot(magic); // codes are: RB_AUTOBOOT, RB_HALT_SYSTEM, RB_POWER_OFF, etc
	FATAL("cannot reboot/shutdown\n");
}

void reboot_recovery(void)
{
	FILE *misc = fopen("/dev/mmcblk0p3","w");
	if (misc) {
		fprintf(misc,"boot-recovery");
		fclose(misc);
	}
	init_reboot(RB_AUTOBOOT);
}

#ifdef SHELL
void shell(void)
{
	fflush(stdout);
	char *sh_argv[] = SHELL_ARGS;
	pid_t pid;
	if (!(pid = fork())) {
		take_console_control();
		execv(BUSYBOX, sh_argv);
	}
	waitpid(pid,NULL,0);
	take_console_control();
}
#endif

void screenshot()
{
	nc_save();
	pid_t pid;
	if (!(pid = fork())) {
		take_console_control();
		execl("/bin/busybox","cp", "-p", "/dev/fb0", "/data/fb0.dump", NULL);
	}
	waitpid(pid,NULL,0);
	take_console_control();
	nc_load();
}

int main(int argc, char **argv, char **envp)
{
	int i,data_dir_to_parse;
	menu_entry *list=NULL,*item;

	// errors before open_console are fatal
	fatal_error = data_dir_to_parse = 1;

	/* mount sys
	 * if the initrd does not contain /sys, make it for them
	 * we want to try as hard as we can to open the console
	 * without the console we can not communicate to the user
	 */
	mkdir("/sys", 0700);
	if(mount("sysfs","/sys","sysfs",MS_RELATIME,""))
		goto error;
	// open the console
	if(open_console())
	{
		umount("/sys");
		goto error;
	}
	umount("/sys");
	if(nc_init())
		goto error;

	fb_init();

	nc_status("mounting /proc");
	// mount proc ( required by kexec )
	if(mount("proc","/proc","proc",MS_RELATIME,""))
	{
		FATAL("cannot mount proc\n");
		goto error;
	}
	nc_status("mounting /data");
	// mount DATA_DEV partition into /data
	if(mount(DATA_DEV,"/data","ext4",0,""))
	{
		FATAL("mounting %s on \"/data\" - %s\n",DATA_DEV,strerror(errno));
		goto error;
	}

	fatal_error=0;

	fb_background();
	if(fatal_error)
	{
		FATAL("fatal error occourred in fb_background() - %s\n",strerror(errno));
		goto error;
	}

	// check for a default entry
	if(parser(DEFAULT_CONFIG,DEFAULT_CONFIG_NAME,&list) && fatal_error)
	{
		umount("/data");
		goto error;
	}

	if(list)
	{
		INFO("found a default config\n");
		if(nc_wait_for_keypress())
		{
			i=MENU_DEFAULT;
			goto skip_menu;
		}
	}
	else
		INFO("no default config found\n");

menu_prompt:
	if(data_dir_to_parse)
	{
		INFO("parsing data directory\n");
		data_dir_to_parse=0;
		if(parse_data_directory(&list))
		{
			umount("/data");
			goto error;
		}
		umount("/data");
		if(nc_compute_menu(list))
			goto error;
	}
	i=nc_get_user_choice();
	//take_console_control();
skip_menu:
	DEBUG("user chose %d\n",i);
	// decide what to do
	switch (i)
	{
		case MENU_FATAL_ERROR:
			FATAL("ncurses - %s\n",strerror(errno));
			goto error;
			break;
		case MENU_DEFAULT:
			if(!list)
			{
				WARN("invalid choice\n");
				goto error; // user can choose one of the default entries ( like recovery )
			}
			item=list;
			break;
		case MENU_REBOOT:
			init_reboot(RB_AUTOBOOT);
			goto error;
		case MENU_HALT:
			init_reboot(RB_POWER_OFF);
			goto error;
		case MENU_RECOVERY:
			reboot_recovery();
			goto error;
#ifdef SHELL
		case MENU_SHELL:
			nc_save();
			printf("\033[2J\033[H"); // clear the screen
			shell();
			nc_load();
			goto menu_prompt;
#endif
		case MENU_SCREENSHOT:
			if(mount(DATA_DEV,"/data","ext4",0,""))
			{
				ERROR("mounting %s on \"/data\" - %s\n",DATA_DEV,strerror(errno));
				goto error;
			}
			screenshot();
			DEBUG("Framebuffer saved to /data/fb0.dump\n");
			umount("/data");
			goto menu_prompt;
		default: // parsed config
			item=get_item_by_id(list,i);
	}
	if(!item)
	{
		WARN("invalid choice\n");
		goto error;
	}
	if(wait_for_device(item->blkdev))
	{
		ERROR("device \"%s\" not found\n",item->blkdev);
		goto error;
	}
	// mount blkdev on NEWROOT
	if(mount(item->blkdev,NEWROOT,"ext4",0,""))
	{
		ERROR("unable to mount \"%s\" on %s - %s\n",item->blkdev,NEWROOT,strerror(errno));
		goto error;
	}
	if(k_load(item->kernel,item->initrd,item->cmdline))
	{
		ERROR("unable to load guest kernel\n");
		umount(NEWROOT);
		goto error;
	}
	umount(NEWROOT);
	if(data_dir_to_parse)
		umount("/data");
	DEBUG("kernel = \"%s\"\n",item->kernel);
	DEBUG("initrd = \"%s\"\n",item->initrd);
	DEBUG("cmdline = \"%s\"\n",item->cmdline);

	// we made it, time to clean up and kexec
	INFO("booting \"%s\"\n",item->name);

	umount("/proc");
	cleanup(data_dir_to_parse, list);
	if(!fork())
	{
		k_exec(); // bye bye
		exit(EXIT_FAILURE);
	}
	wait(NULL); // should not return on success
	take_console_control();
	//make sure that user read this
	nc_error("kexec call failed!");
	exit(EXIT_FAILURE); // kernel panic here

error:
	if(!fatal_error)
		goto menu_prompt;
	cleanup(data_dir_to_parse, list);

/* // uncomment this if you want, it may be useful, but it can also be annoying.
#ifdef SHELL
	printf("\033[2J\033[HAn error occurred during development. Dropping to an emergency shell.\n\n");
	shell();
#endif*/

	printf("Kernel panic in 10 seconds...");
	fflush(stdout);
	exit(EXIT_FAILURE);
}