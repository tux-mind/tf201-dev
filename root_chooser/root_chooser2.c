#include "root_chooser2.h"
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

int main(int argc, char **argv, char **envp) //TODO: should declare it as void ?
{
	FILE *log,*input;
	char 	*line, // where we place the readed line
				*root, // directory to chroot
				*blkdev, // block device to mount on newroot
				*pos, // current position
				**new_argv; // init args
	int i; // used for loops


#ifdef ADB
	//provide ADB access
	if(!fork())
		fatal(argv,envp);
#endif

	if((log = fopen(LOG,"w")) != NULL)
	{
		if(!mount("sysfs","/sys","sysfs",MS_RELATIME,"")) // mount sys
		{
			mdev(envp);
			umount("/sys");
			//mount DATA_DEV partition into /data
			if(!mount(DATA_DEV,"/data","ext4",0,""))
			{
				if((input = fopen(BOOT_FILE,"r")) != NULL)
				{
					if((line = malloc(MAX_LINE)) != NULL && (blkdev = malloc(MAX_LINE)) != NULL && (root = malloc(MAX_LINE)) != NULL && (new_argv = malloc(2*sizeof(char*))) != NULL && (new_argv[0] = malloc(MAX_LINE)) != NULL)
					{
						if(fgets(line,MAX_LINE,input))
						{
							strcpy(root,"/newroot/");
							for(i=0,pos=line;*pos!=':'&&*pos!='\0'&&*pos!='\n';pos++)
								blkdev[i++] = *pos;
							blkdev[i] ='\0';
							if(*pos==':')
								pos++;
							if(*pos=='/')
								pos++;
							for(i=9;*pos!=':'&&*pos!='\0'&&*pos!='\n';pos++)
								root[i++] = *pos;
							root[i] = '\0';
							if(*pos==':')
								pos++;
							for(i=0;*pos!='\0'&&*pos!='\n';pos++)
								new_argv[0][i++] = *pos;
							new_argv[1] = NULL;
							free(line);
							fclose(input);
							umount("/data");
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
							if(!mount(blkdev,NEWROOT,"ext4",0,""))
							{
								free(blkdev);
								if(!chdir(root) && !chroot(root))
								{
									free(root);
									if(!access(new_argv[0],X_OK))
									{
										fclose(log);
										execve(new_argv[0],new_argv,envp);
									}
									else
									{
										fprintf(log,"cannot execute \"%s\" - %s\n",new_argv[0],strerror(errno));
										free(new_argv[0]);
										free(new_argv);
										fclose(log);
										umount(NEWROOT);
									}
								}
								else
								{
									fprintf(log,"unable to chdir/chroot to \"%s\" - %s\n",root,strerror(errno));
									free(root);
									free(new_argv[0]);
									free(new_argv);
									fclose(log);
									umount(NEWROOT);
								}
							}
							else
							{
								fprintf(log,"unable to mount \"%s\" on %s - %s\n",blkdev,NEWROOT,strerror(errno));
								free(blkdev);
								free(root);
								free(new_argv[0]);
								free(new_argv);
								fclose(log);
							}
						}
						else
						{
							fprintf(log,"while reading \"%s\" - %s\n",BOOT_FILE,strerror(errno));
							free(line);
							free(blkdev);
							free(root);
							free(new_argv[0]);
							free(new_argv);
							fclose(input);
							umount("/data");
							fclose(log);
						}
					}
					else
					{
						//TODO: check for single malloc calls and free the 'done well' calls results.
						fprintf(log,"malloc - %s\n",strerror(errno));
						fclose(input);
						umount("/data");
						fclose(log);
					}
				}
				else
				{
					fprintf(log,"unable to open \"%s\" - %s\n",BOOT_FILE,strerror(errno));
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
	return EXIT_FAILURE; // just for make gcc don't fire up warnings
}
