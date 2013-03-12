#include "root_chooser6.h"

// if == 1 => someone called FATAL we have to exit
int fatal_error;

// fatal error occourred, boot up android
void fatal(char **argv,char **envp)
{
	// TODO: add additional error checking?
	// lock android boot if wanna only adb
#ifndef ADB
	chdir(NEWROOT);
#endif
	chroot(NEWROOT);
	execve("/init",argv,envp);
}

/* make /dev from /sys */
void mdev()
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

/* parse line as "blkdev:kernel:initrd"
 * returned values are:
 *	0 if ok
 *	1 if a malloc error occourred or
 *    if a parse error occourred.
 * 	if an error occoured, errno is set to the corresponding error number.
 */
int parser(char *line,char **blkdev, char**kernel, char **initrd)
{
	register char *pos;
	register int i;

	// count args length
	for(i=0,pos=line;*pos!=':'&&*pos!='\0';pos++)
		i++;
	// check arg length
	if(!i)
	{
		errno = EINVAL;
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
		errno = EINVAL;
		ERROR("missing kernel\n");
		return 1;
	}
	*kernel = malloc((i+NEWROOT_STRLEN+1)*sizeof(char));
	if(!*kernel)
	{
		free(*blkdev);
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
			FATAL("malloc - %s\n",strerror(errno));
			return 1;
		}
		// append the read value to NEWROOT
		strncpy(*initrd,NEWROOT,NEWROOT_STRLEN);
		strncpy(*initrd+NEWROOT_STRLEN,pos - i,i);
		*(*initrd + NEWROOT_STRLEN+i) = '\0';
	}
	else
		*initrd=NULL;
	return 0; // everyting is ok
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
	return len*sizeof(char);
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

	*cmdline = malloc((len+2)*sizeof(char));
	if(!cmdline)
	{
		FATAL("malloc - %s\n",strerror(errno));
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
	*(*cmdline +len) = '\n';
	*(*cmdline +len+1) = '\0';
	return 0;
}

void take_console_control()
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
int open_console()
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

char getch() {
        char buf = 0;
        struct termios old = {0};
        if (tcgetattr(0, &old) < 0)
                return -1;
        old.c_lflag &= ~ICANON;
        old.c_lflag &= ~ECHO;
        old.c_cc[VMIN] = 1;
        old.c_cc[VTIME] = 0;
        if (tcsetattr(0, TCSANOW, &old) < 0)
                return -1;
        if (read(0, &buf, 1) < 0)
                return -1;
        old.c_lflag |= ICANON;
        old.c_lflag |= ECHO;
        if (tcsetattr(0, TCSADRAIN, &old) < 0)
                return -1;
        return (buf);
}

void press_enter()
{
	char buff[MAX_LINE];
	INFO("press <ENTER> to continue..."); // the last "\n" is added by the user
	fgets(buff,MAX_LINE,stdin);
}

int wait_for_keypress(int timeout)
{
	int i,stat;
	pid_t pid,wpid;

	if((pid = fork()))
	{
		i=0;
		take_console_control();
    do
		{
			wpid = waitpid(pid, &stat, WNOHANG);
			if (wpid == 0)
			{
				if (timeout > 0) {
					printf("\r\033[1KAutomatic boot in %2u seconds...", 10-i); // rewrite the line every second
					fflush(stdout);
				}
				if (i < timeout)
				{
					sleep(1);
					i++;
				}
				else
					kill(pid, SIGKILL);
			}
    } while (wpid == 0 && i <= timeout);
		take_console_control();
		if(i>timeout || !WIFEXITED(stat))
			stat = -1; // no keypress
		else
			stat = WEXITSTATUS(stat);
		return stat;
	}
	else if(pid < 0)
	{
		FATAL("cannot fork - %s\n",strerror(errno));
		return 0;
	}
	else
	{
		take_console_control();
		printf("\r\033[2K");
		exit( getch() );
		return 0; /* not reached */
	}
}

int get_user_choice()
{
	int i,stat;
	char buff[MAX_LINE];
	pid_t pid;

	printf("enter a number and press <ENTER>: ");
	fflush(stdout);

	if(!(pid = fork()))
	{
		take_console_control();
		fgets(buff,MAX_LINE,stdin);
		DEBUG("child read \"%s\"\n",buff);
		i = atoi(buff);
		exit(i);
		return 0; /* not reached */
	}
	if (pid < 0)
	{
		FATAL("cannot fork - %s\n",strerror(errno));
		return 0;
	}
	else
	{
		waitpid(pid,&stat,0);
		take_console_control();
		stat = WEXITSTATUS(stat);
		return stat;
	}
}

int parse_data_directory(menu_entry **list)
{
	DIR *dir;
	struct dirent *d;
	FILE *file;
	char line[MAX_LINE],*blkdev,*kernel,*cmdline,*initrd,*path;
	int len;

	if((dir = opendir(DATA_DIR)) == NULL)
	{
		ERROR("opening %s - %s\n",DATA_DIR,strerror(errno));
		return -1;
	}
	while((d = readdir(dir)) != NULL)
		if(d->d_type != DT_DIR)
		{
			len = strlen(d->d_name);
			DEBUG("parsing %s [%d]\n",d->d_name,len);
			if((path = malloc((DATA_DIR_STRLEN + len +1)*sizeof(char)))==NULL)
			{
				FATAL("malloc - %s\n",strerror(errno));
				closedir(dir);
				return -1;
			}
			snprintf(path,DATA_DIR_STRLEN+len+1,"%s%s",DATA_DIR,d->d_name);
			if((file = fopen(path,"r")) == NULL)
			{
				ERROR("opening \"%s\" - %s\n",path,strerror(errno));
				closedir(dir);
				free(path);
				return -1;
			}
			free(path);
			// check that there is a description, but copy it later
			if((fgets(line,MAX_LINE,file)) == NULL)
			{
				ERROR("no description found\n");
				ERROR("reading %s%s - %s\n",DATA_DIR,d->d_name,strerror(errno));
				fclose(file);
				closedir(dir);
				return -1;
			}
			if((fgets(line,MAX_LINE,file)) == NULL)
			{
				ERROR("reading %s%s - %s\n",DATA_DIR,d->d_name,strerror(errno));
				fclose(file);
				closedir(dir);
				return -1;
			}
			fgets_fix(line);
			if(parser(line,&blkdev,&kernel,&initrd) || cmdline_parser(fgets_fix(fgets(line,MAX_LINE,file)),&(cmdline)))
			{
				fclose(file);
				if(fatal_error)
				{
					closedir(dir);
					return -1;
				}
				press_enter();
				continue;
			}
			rewind(file);
			if(fgets(line,MAX_LINE,file) == NULL || strlen(line) == 0) // no name provided
				*list = add_entry(*list,d->d_name,blkdev,kernel,cmdline,initrd);
			else
			{
				fgets_fix(line);
				DEBUG("description: %s\n",line);
				*list = add_entry(*list,line,blkdev,kernel,cmdline,initrd);
			}
			fclose(file);
		}
	closedir(dir);
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

int check_for_default_config(menu_entry **def_entry)
{
	FILE *fin;
	char buff[MAX_LINE];

	*def_entry = NULL;
	if((fin=fopen(ROOT_FILE,"r")))
	{
		if((*def_entry = malloc(sizeof(menu_entry))))
		{
			if(fgets(buff,MAX_LINE,fin) && fgets(buff,MAX_LINE,fin)) // read the second line
			{
				fgets_fix(buff);
				if (
					!parser(buff,&((*def_entry)->blkdev),&((*def_entry)->kernel),&((*def_entry)->initrd)) &&
					!cmdline_parser(fgets_fix(fgets(buff,MAX_LINE,fin)),&((*def_entry)->cmdline))
				)
				{
					(*def_entry)->name = "default";
					DEBUG("have a default entry\n");
				}
				else
				{
					free(*def_entry);
					*def_entry = NULL;
					if(fatal_error)
						return -1;
				}
			}
			else
			{
				free(*def_entry);
				*def_entry = NULL;
			}
		}
		else
		{
			FATAL("malloc - %s\n",strerror(errno));
			return -1;
		}
		fclose(fin);
	}
	return 0;
}

void init_reboot(int magic)
{
	// this could use a lot more cleanup (unmount, etc)
	reboot(magic); // codes are: RB_AUTOBOOT, RB_HALT_SYSTEM, RB_POWER_OFF, etc
	exit(0); // should not return on success
}

void reboot_recovery() {
	FILE *misc = fopen("/dev/mmcblk0p3","w");
	if (misc) {
		fprintf(misc,"boot-recovery");
		fclose(misc);
	}
	init_reboot(RB_AUTOBOOT);
}

#ifdef SHELL
void shell(char **envp) {
	char *sh_argv[] = SHELL_ARGS;
	pid_t pid;
	if (!(pid = fork())) {
		take_console_control();
		execve(BUSYBOX, sh_argv, envp);
	}
	waitpid(pid,NULL,0);
	take_console_control();
}
#endif

int main(int argc, char **argv, char **envp)
{
	int i;
	menu_entry *list=NULL,*item,*def_entry=NULL;
	char *kernel,*initrd,*cmdline;

	// errors before open_console are fatal
	fatal_error = 1;

	// mount sys
	if(mount("sysfs","/sys","sysfs",MS_RELATIME,""))
		goto error;
	// open the console ( this is required from version 5 )
	if(open_console())
	{
		umount("/sys");
		goto error;
	}
	umount("/sys");
	// automatically boot in X seconds
	if (wait_for_keypress(TIMEOUT_BOOT) == -1)
		goto android; // boot android (TODO: boot default config)
	
	// init printed_lines counter and fatal error flag
	fatal_error=printed_lines=0;
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
	if(check_for_default_config(&def_entry))
		goto error;
	DEBUG("parsing %s\n",DATA_DIR);
	if(parse_data_directory(&list))
	{
		umount("/data");
		goto error;
	}
	umount("/data");

	/* we restart from here in case of not fatal errors */
menu_prompt:
	//printf("\033[2J\033[H"); // this will clear the whole screen (sorry penguins)
	print_menu(list);
	i=get_user_choice();
	DEBUG("user chose %d\n",i);

	// decide what to do
	switch (i) {
		case 0: // android
			INFO("booting android\n");
			fatal_error = 1; // force fatal() call
			goto error;
		case 1: // reboot
			init_reboot(RB_AUTOBOOT);
			goto error;
		case 2: // halt
			init_reboot(RB_HALT_SYSTEM);
			goto error;
		case 3: // reboot recovery
			reboot_recovery();
			goto error;
#ifdef SHELL
		case 4: // busybox sh
			shell(envp);
			goto menu_prompt;
#endif
		default: // parsed config
			if ((i==-1) && (!def_entry))
			{
				WARN("no default entry found, please make a choice");
				goto error;
			}
			else
				item=get_item_by_id(list,i);
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
			
			// we made it, time to clean up and chroot
			DEBUG("mounted \"%s\" on \"%s\"\n",item->blkdev,NEWROOT);
			INFO("booting \"%s\"\n",item->name);

			// store args locally
			kernel = item->kernel;
			initrd = item->initrd;
			cmdline = item->cmdline;
			// set to NULL to avoid free() from free_menu()
			item->initrd = item->kernel = item->cmdline = NULL;

			free_menu(list);
			if(def_entry!=NULL)
			{
				def_entry->name = NULL;
				free_entry(def_entry);
			}
			if(!fork())
			{
				kexec(kernel,initrd,cmdline); // reboot into the new kernel
				exit(-1);
			}
			wait(NULL); // should not return on success
			take_console_control();
			FATAL("failed to kexec\n"); // we cannot go back, already freed everything...
	}

error:
	press_enter(); // TODO: no need to pause for android
	if(!fatal_error)
		goto menu_prompt;
	free_menu(list);
	if(def_entry)
	{
		def_entry->name = NULL;
		free_entry(def_entry);
	}
	umount("/proc");
android:
	fatal(argv,envp);
	exit(EXIT_FAILURE);
}
