#include "root_chooser4.h"
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
 *	1 if android boot_option is found or
 *		if a malloc error occourred or
 *		if a parse error occourred.
 * 	if an error occour errno it's set to the corresponding error number.
 */
int parser(char *line,char **blkdev, char **root, char ***init_args)
{
	register char *pos;
	char *args_pool[INIT_MAX_ARGS+1];
	register int i;
	int j;

	if(line[0]=='#')
		return 1;

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

int main(int argc, char **argv, char **envp)
{
	FILE *log,*input;
	char 	*line, // where we place the readed line
				*root, // directory to chroot
				*blkdev, // block device to mount on newroot
				**new_argv, // init args
				*input_file_path; // input file path
	int i; // general purpose integer


#ifdef ADB
	//provide ADB access
	if(!fork())
		fatal(argv,envp);
#endif

	line = blkdev = root = NULL;
	new_argv = NULL;
	i=0;

	if((log = fopen(LOG,"w")) != NULL)
	{
		if(!mount("sysfs","/sys","sysfs",MS_RELATIME,"")) // mount sys
		{
			mdev(envp);
			umount("/sys");
			//mount DATA_DEV partition into /data
			if(!mount(DATA_DEV,"/data","ext4",0,""))
			{
				if(!access(ROOT_TMP_FILE,R_OK))
				{
					input_file_path = ROOT_TMP_FILE;
					i = 1; // use i for set that we have to delete the input file
				}
				else
					input_file_path = ROOT_FILE;

				if((input = fopen(input_file_path,"r")) != NULL)
				{
					if((line = malloc(MAX_LINE)) != NULL)
					{
						if(fgets(line,MAX_LINE,input))
						{
							fclose(input);
							if(i)
								unlink(input_file_path);
							umount("/data");
							fgets_fix(line);
							if(!parser(line,&blkdev,&root,&new_argv))
							{
								free(line);
								//check if blkdev exist, otherwise wait until TIMEOUT
								if(access(blkdev,R_OK) && !mount("sysfs","/sys","sysfs",MS_RELATIME,""))
								{
									sleep(1);
									mdev(envp);
									for(i=1;access(blkdev,R_OK) && i < TIMEOUT;i++)
									{
										sleep(1);
										mdev(envp);
									}
									umount("/sys");
								}
								//mount blkdev on NEWROOT
								if(!mount(blkdev,NEWROOT,"ext4",0,""))
								{
									free(blkdev);
									//if root is a ext image mount it on NEWROOT
									if(!try_loop_mount(&root,NEWROOT))
									{
										//check for init existence
										i=strlen(root) + strlen(new_argv[0]);
										if((line=malloc((i+1)*sizeof(char))))
										{
											strncpy(line,root,i);
											strncat(line,new_argv[0],i);
											line[i]='\0';
											if(!access(line,R_OK|X_OK))
											{
												free(line);
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
										}
									}
									else
									{
										if(root) // try_loop_mount reallocate root, a malloc problem can be happend
										{
											fprintf(log,"try_loop_mount \"%s\" on %s - %s\n",root,NEWROOT,strerror(errno));
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
								if(line[0] != '#')
									fprintf(log,"parsing failed - %s\n",strerror(errno));
								else
									unlink(LOG);
								free(line);
								fclose(log);
							}
						}
						else
						{
							fprintf(log,"while reading \"%s\" - %s\n",input_file_path,strerror(errno));
							free(line);
							fclose(input);
							umount("/data");
							fclose(log);
						}
					}
					else // this should never happen but i love error checking ;)
					{
						fprintf(log,"malloc - %s\n",strerror(errno));
						fclose(input);
						umount("/data");
						fclose(log);
					}
				}
				else
				{
					fprintf(log,"unable to open \"%s\" - %s\n",input_file_path,strerror(errno));
					umount("/data");
					fclose(log);
				}
			}
			else
			{
				fprintf(log,"unable to mount %s on /data - %s\n",DATA_DEV,strerror(errno));
				fclose(log);
			}
		}
		else
		{
			fprintf(log, "unable to mount /sys - %s\n",strerror(errno));
			fclose(log);
		}
	}

	fatal(argv,envp);
	exit(EXIT_FAILURE);
}
