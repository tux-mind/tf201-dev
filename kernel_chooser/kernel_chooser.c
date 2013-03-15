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
#include "menu2.h"
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
 * @def_entry: optional pointer to the menu_entry struct
 */
int parser(char *file, char *fallback_name, menu_entry **list, menu_entry **def_entry)
{
	FILE *fin;
	char name_line[MAX_NAME],line[MAX_LINE],*blkdev,*kernel,*initrd,*cmdline,*name;
	int name_len;

	blkdev=kernel=initrd=cmdline=name=NULL;

	if(!(fin=fopen(file,"r")))
	{
		// if we are reading the default entry this isn't an error
		if(!def_entry)
			ERROR("cannot open \"%s\" - %s\n", file,strerror(errno));
		//nothing to free, exit now
		return -1;
	}

	if(!fgets(name_line,MAX_NAME,fin) || !fgets(line,MAX_LINE,fin)) // read the second line
	{
		fclose(fin);
		// error
		if(!feof(fin))
		{
			ERROR("cannot read \"%s\" - %s\n",file,strerror(errno));
			return -1;
		}
		WARN("file \"%s\" must have at least 2 lines\n",file);
		return -1;
	}
	fgets_fix(name_line);
	fgets_fix(line);
	name_len = strlen(name_line);
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
		config_parser(line,&blkdev,&kernel,&initrd) ||
		cmdline_parser(fgets_fix(fgets(line,MAX_LINE,fin)),&cmdline)
	)
		goto error_with_fclose;
	fclose(fin);
	if(!def_entry)
	{
		*list = add_entry(*list, name, blkdev, kernel, cmdline, initrd);
		return 0;
	}
	// we are building the default entry, which isn't in the list
	*def_entry = malloc(sizeof(menu_entry));
	if(!(*def_entry))
	{
		FATAL("malloc - %s\n",strerror(errno));
		goto error;
	}
	(*def_entry)->name 		= name;
	(*def_entry)->kernel 	= kernel;
	(*def_entry)->initrd 	= initrd;
	(*def_entry)->blkdev 	= blkdev;
	(*def_entry)->cmdline	= cmdline;
	(*def_entry)->next = NULL;
	have_default=1;
	return 0;

error_with_fclose:
	fclose(fin);
error:
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
		for(i=1;access(CONSOLE,R_OK|W_OK) && i < TIMEOUT;i++)
		{
			sleep(1);
			mdev();
		}
		if(i==TIMEOUT) // no console availbale ( user it's using an older kernel )
		{
			errno = ETIMEDOUT;
			return -1;
		}
	}
	take_console_control();
	return 0;
}

char getch()
{
	char buf = 0;
	struct termios old,new;

	if(tcgetattr(STDIN_FILENO, &old)<0)
		return -1;
	new = old;
	new.c_lflag &= ~(ICANON | ECHO);
	new.c_cc[VMIN] = 1;
	new.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSANOW, &new) < 0)
					return -1;
	if (read(STDIN_FILENO, &buf, 1) < 0)
					return -1;
	if (tcsetattr(0, TCSADRAIN, &old) < 0)
					return -1;
	return buf;
}

void press_enter(void)
{
	char garbage[MAX_LINE];
	INFO("press <ENTER> to continue..."); // the last "\n" is added by the user
	fgets(garbage,MAX_LINE,stdin);
}

int wait_for_keypress(void)
{
	int stat,timeout;
	pid_t pid,wpid;

	timeout = TIMEOUT_BOOT;

	if((pid = fork()))
	{
    do
		{
			wpid = waitpid(pid, &stat, WNOHANG);
			if(!wpid)
			{
					printf("\r\033[1KAutomatic boot in %2u seconds...", timeout); // rewrite the line every second
					fflush(stdout);
					timeout--;
					sleep(1);
			}
    } while (wpid == 0 && timeout);
		if(wpid== 0 || !timeout || !WIFEXITED(stat))
		{
			if(!wpid)
				kill(pid, SIGKILL);
			stat = MENU_DEFAULT_NUM; // no keypress
		}
		else
			stat = WEXITSTATUS(stat);
		printf("\r\033[2K");
		return stat;
	}
	else if(pid < 0)
	{
		FATAL("cannot fork - %s\n",strerror(errno));
		return 0;
	}
	else
	{
		exit( getch() );
		return 0; /* not reached */
	}
}

int get_user_choice(void)
{
	int i;
	char buff[MAX_LINE];

	printf("enter a number and press <ENTER>: ");

	fgets(buff,MAX_LINE,stdin);
	fgets_fix(buff);
	for(i=0;i<MAX_LINE && buff[i] != '\0' && isspace(buff[i]);i++);
	switch(buff[i])
	{
		case MENU_DEFAULT:
			i = MENU_DEFAULT_NUM;
			break;
		case MENU_REBOOT:
			i = MENU_REBOOT_NUM;
			break;
		case MENU_HALT:
			i=MENU_HALT_NUM;
			break;
		case MENU_RECOVERY:
			i=MENU_RECOVERY_NUM;
			break;
#ifdef SHELL
		case MENU_SHELL:
			i=MENU_SHELL_NUM;
			break;
#endif
		default:
			i = atoi(buff);
	}
	printed_lines++; // user press enter
	return i;
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
			DEBUG("parsing %s\n",d->d_name);
			if(parser(d->d_name,d->d_name,list,NULL))
			{
				if(fatal_error)
				{
					closedir(dir);
					chdir("/");
					return -1;
				}
				press_enter();
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
		for(i=1;access(blkdev,R_OK) && i < TIMEOUT;i++)
		{
			sleep(1);
			mdev();
		}
		umount("/sys");
		if(i==TIMEOUT)
			return -1;
	}
	return 0;
}

void init_reboot(int magic)
{
	// this could use a lot more cleanup (unmount, etc)
	reboot(magic); // codes are: RB_AUTOBOOT, RB_HALT_SYSTEM, RB_POWER_OFF, etc
	FATAL("cannot reboot/shutdown\n");
	exit(-1);
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

int main(int argc, char **argv, char **envp)
{
	int i;
	menu_entry *list=NULL,*item,*def_entry=NULL;

	// errors before open_console are fatal
	fatal_error = 1;

	// mount sys
	if(mount("sysfs","/sys","sysfs",MS_RELATIME,""))
		goto error;
	// open the console
	if(open_console())
	{
		umount("/sys");
		goto error;
	}
	umount("/sys");

	// init printed_lines counter, fatal error flag and default entry flag
	fatal_error=printed_lines=have_default=0;
	printf(HEADER);
	// mount proc ( required by kexec )
	if(mount("proc","/proc","proc",MS_RELATIME,""))
	{
		FATAL("cannot mount proc\n");
		goto error;
	}
	INFO("mounting /data\n");
	// mount DATA_DEV partition into /data
	if(mount(DATA_DEV,"/data","ext4",0,""))
	{
		FATAL("mounting %s on \"/data\" - %s\n",DATA_DEV,strerror(errno));
		goto error;
	}
	// check for a default entry
	if(parser(DEFAULT_CONFIG,"default",NULL,&def_entry) && fatal_error)
	{
		umount("/data");
		goto error;
	}
	if(!have_default)
		INFO("no default config found\n");
	if(parse_data_directory(&list))
	{
		umount("/data");
		goto error;
	}
	umount("/data");
	// we have printed something, give user time to read
	// ( >1 because INFO(mounting data) is not useful )
	if(printed_lines>1)
		press_enter();
	clear_screen();
	// automatically boot in TIMEOUT_BOOT seconds
	if (have_default && ((i=wait_for_keypress()) == MENU_DEFAULT_NUM))
		goto skip_menu; // automatic boot ( no input required )

	// now we have all data. ( NOTE: 'i' contains the pressed key if needed )

	/* we restart from here in case of not fatal errors */
menu_prompt:
	print_menu(list);
	i=get_user_choice();
skip_menu:
	DEBUG("user chose %d\n",i);
	// decide what to do
	switch (i)
	{
		case MENU_DEFAULT_NUM:
			if(!have_default)
			{
				WARN("invalid choice\n");
				goto error;
			}
			item=def_entry;
			break;
		case MENU_REBOOT_NUM:
			init_reboot(RB_AUTOBOOT);
			goto error;
		case MENU_HALT_NUM:
			init_reboot(RB_HALT_SYSTEM);
			goto error;
		case MENU_RECOVERY_NUM:
			reboot_recovery();
			goto error;
#ifdef SHELL
		case MENU_SHELL_NUM:
			shell();
			goto menu_prompt;
#endif
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
	DEBUG("kernel = \"%s\"\n",item->kernel);
	DEBUG("initrd = \"%s\"\n",item->initrd);
	DEBUG("cmdline = \"%s\"\n",item->cmdline);

	// we made it, time to clean up and kexec
	INFO("booting \"%s\"\n",item->name);

	#ifdef DEVELOPMENT
	press_enter();
	#endif
	free_menu(list);
	if(have_default)
		free_entry(def_entry);
	if(!fork())
	{
		k_exec(); // bye bye
		exit(EXIT_FAILURE);
	}
	wait(NULL); // should not return on success
	take_console_control();
	FATAL("failed to kexec\n"); // we cannot go back, already freed everything...
	FATAL("this is horrible!\n");
	FATAL("please provide a full bug report to developers\n");
	press_enter();
	exit(EXIT_FAILURE); // kernel panic here

error:
	press_enter();
	if(!fatal_error)
		goto menu_prompt;
	free_menu(list);
	if(have_default)
		free_entry(def_entry);
	umount("/proc");
	exit(EXIT_FAILURE);
}
