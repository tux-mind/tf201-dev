#include "root_chooser5.h"
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

/* parse line as "blkdev:root_directory:init_path init_args"
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
		while(*pos==' ')
			pos++;
		for(i=0;*pos!='\0'&&*pos!=' ';pos++)
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

int open_console(char **envp)
{
	int i;

	mdev(envp);
	if(access(CONSOLE,R_OK|W_OK))
	{ // no console yet...wait until timeout
		sleep(1);
		for(i=1;access(CONSOLE,R_OK|W_OK) && i < TIMEOUT;i++)
		{
			sleep(1);
			mdev(envp);
		}
		if(i==TIMEOUT) // no console availbale ( user it's using an older kernel )
		{
			errno = ETIMEDOUT;
			return -1;
		}
	}
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

void press_enter()
{
	char buff[MAX_LINE];
	INFO("press <ENTER> for continue...");// the last "\n" is putted by the user
	fgets(buff,MAX_LINE,stdin);
}

int get_user_choice()
{
	int i,stat,timeout;
	char buff[MAX_LINE];
	pid_t pid,wpid;

	INFO("enter a number and press <ENTER>: ");
	fflush(stdout);
	timeout = TIMEOUT*2;

	if((pid = fork()))
	{
		i=0;
    do
		{
			wpid = waitpid(pid, &stat, WNOHANG);
			if (wpid == 0)
			{
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
		{
			stat = -1; // boot default
			printf("\n"); // user don't press enter.
		}
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
		fgets(buff,MAX_LINE,stdin);
		DEBUG("child read \"%s\"\n",buff);
		sscanf(buff,"%d",&i);
		exit(i);
		return 0; /* not reahced */
	}
}

int parse_data_directory(menu_entry **list)
{
	DIR *dir;
	struct dirent *d;
	FILE *file;
	char line[MAX_LINE],*blkdev,*root,**init_args,*path;
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
			if((fgets(line,MAX_LINE,file)) == NULL)
			{
				ERROR("reading %s%s - %s\n",DATA_DIR,d->d_name,strerror(errno));
				fclose(file);
				closedir(dir);
				return -1;
			}
			fgets_fix(line);
			if(parser(line,&blkdev,&root,&init_args))
			{
				fclose(file);
				WARN("parsing %s%s - %s\n",DATA_DIR,d->d_name,strerror(errno));
				press_enter();
				continue;
			}
			if(fgets(line,MAX_LINE,file) == NULL || strlen(line) == 0) // no name provided
				*list = add_entry(*list,d->d_name,blkdev,root,init_args);
			else
			{
				fgets_fix(line);
				DEBUG("description: %s\n",line);
				*list = add_entry(*list,line,blkdev,root,init_args);
			}
			fclose(file);
		}
	closedir(dir);
	return 0;
}

int main(int argc, char **argv, char **envp)
{
	int i;
	menu_entry *list=NULL,*item,*def_entry=NULL;
	char *init_abs_path,**final_init_args,buff[MAX_LINE];
	int loop_device_mounted;
	FILE *fin;

	printed_lines=0; // init printed_lines counter
	//mount sys
	if(!mount("sysfs","/sys","sysfs",MS_RELATIME,""))
	{
		if(!open_console(envp))
		{
			umount("/sys");
			//mount DATA_DEV partition into /data
			printf(HEADER);
			INFO("mounting /data\n");
			if(!mount(DATA_DEV,"/data","ext4",0,""))
			{
				def_entry = NULL;
				if((fin=fopen(ROOT_FILE,"r")))
				{
					if((def_entry = malloc(sizeof(menu_entry))))
					{
						if(fgets(buff,MAX_LINE,fin))
						{
							fgets_fix(buff);
							if(!parser(buff,&(def_entry->blkdev),&(def_entry->root),&(def_entry->init_argv)))
							{
								def_entry->name = "default";
								DEBUG("have a default entry\n");
							}
							else
							{
								free(def_entry);
								def_entry = NULL;
							}
						}
						else
						{
							free(def_entry);
							def_entry = NULL;
						}
					}
					fclose(fin);
				}
				DEBUG("parsing %s\n",DATA_DIR);
				if(!parse_data_directory(&list))
				{
					umount("/data");
#ifdef STOP_BEFORE_MENU
					press_enter();
#endif
					menu_prompt:
					print_menu(list);
					if((i=get_user_choice()) && (def_entry || i!=-1))
					{
						DEBUG("user choose %d\n",i);
						if(i==-1)
							item=def_entry;
						else
							item=get_item_by_id(list,i);
						if(item)
						{
							if(access(item->blkdev,R_OK) && !mount("sysfs","/sys","sysfs",MS_RELATIME,""))
							{
								DEBUG("block device \"%s\" not found.\n",item->blkdev);
								DEBUG("rebuilding /dev\n");
								sleep(1);
								mdev(envp);
								for(i=1;access(item->blkdev,R_OK) && i < TIMEOUT;i++)
								{
									sleep(1);
									mdev(envp);
								}
								umount("/sys");
							}
							//mount blkdev on NEWROOT
							if(!mount(item->blkdev,NEWROOT,"ext4",0,""))
							{
								DEBUG("mounted \"%s\" on \"%s\"\n",item->blkdev,NEWROOT);
								loop_device_mounted = 0;
								init_abs_path = item->root; // thus to have a trace of the old pointer
								//if root is an ext image mount it on NEWROOT
								if(!try_loop_mount(&(item->root),NEWROOT))
								{
									if(item->root != init_abs_path)
										loop_device_mounted=1;
									//check for init existence
									i=strlen(item->root) + strlen((item->init_argv)[0]);
									if((init_abs_path=malloc((i+1)*sizeof(char))))
									{
										strncpy(init_abs_path,item->root,i);
										strncat(init_abs_path,(item->init_argv)[0],i);
										init_abs_path[i]='\0';
										if(!access(init_abs_path,R_OK|X_OK))
										{
											free(init_abs_path);
											final_init_args = item->init_argv;
											//chroot
											if(!chdir(item->root) && !chroot(item->root))
											{
												INFO("booting \"%s\"\n",item->name);
												item->init_argv=NULL;
												free_menu(list);
												if(def_entry!=NULL)
												{
													def_entry->name = NULL;
													free_entry(def_entry);
												}
												execve(final_init_args[0],final_init_args,envp);
											}
											else
											{
												ERROR("cannot chroot/chdir to \"%s\" - \"%s\"",item->root,strerror(errno));
												umount(NEWROOT);
												if(loop_device_mounted)
													umount(NEWROOT);
												press_enter();
												goto menu_prompt;
											}
										}
										else
										{
											ERROR("cannot execute \"%s\" - %s\n",init_abs_path,strerror(errno));
											free(init_abs_path);
											umount(NEWROOT);
											if(loop_device_mounted)
												umount(NEWROOT);
											press_enter();
											goto menu_prompt;
										}
									}
									else
									{
										FATAL("malloc - %s\n",strerror(errno));
										free_menu(list);
										umount(NEWROOT);
										if(loop_device_mounted)
											umount(NEWROOT);
										press_enter();
									}
								}
								else
								{
									umount(NEWROOT);
									if(item->root) // try_loop_mount reallocate root, a malloc problem can be happend
									{
										ERROR("try_loop_mount \"%s\" on %s - %s\n",item->root,NEWROOT,strerror(errno));
										press_enter();
										goto menu_prompt;
									}
									else
									{
										FATAL("try_loop_mount: malloc - %s\n",strerror(errno));
										free_menu(list);
										press_enter();
									}
								}
							}
							else
							{
								ERROR("unable to mount \"%s\" on %s - %s\n",item->blkdev,NEWROOT,strerror(errno));
								press_enter();
								goto menu_prompt;
							}
						}
						else
						{
							ERROR("wrong choice\n");
							press_enter();
							goto menu_prompt;
						}
					}
					else
					{
						INFO("booting up android\n");
					}
				}
				else
				{
					umount("/data");
					press_enter();
				}
			}
			else
			{
				ERROR("mounting %s on \"/data\" - %s\n",DATA_DEV,strerror(errno));
				press_enter();
			}
		}
		else
		{
			umount ("/sys");
		}
	}
	if(def_entry)
	{
		def_entry->name = NULL;
		free_entry(def_entry);
	}
	fatal(argv,envp);
	exit(EXIT_FAILURE);
}