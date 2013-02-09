#include "boot_chooser3.h"
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

void main(int argc, char **argv, char **envp)
{
	FILE *log,*input;
	char 	*line, // where we place the readed line
				*root, // directory to chroot
				*blkdev, // block device to mount on newroot
				*pos, // current position
				**new_argv; // init args
	int i, // used for loops
			fd_to_close;
			/* if we use loop devices we have to
			 * keep the /dev/loop* file descriptor
			 * until mount(2) call is done.
			 * BUT we also have to close it after mount is done.
			 * int the mount(8) source code i found that
			 * they don't close that file descriptor at all.
			 * their reason is that mount(8) is a short-lived process,
			 * so exit() call will close that fd automatically.
			 * here we are the init process, the longest-lived process! :)
			 * so, fd_to_close is the /dev/loop file descriptor to close
			 * after the related mount(2) call.
			 */


#ifdef ADB
	//provide ADB access
	if(!fork())
		fatal(argv,envp);
#endif

	fd_to_close = 0;
	line = blkdev = root = NULL;
	new_argv = NULL;

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
							strcpy(root,NEWROOT);
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
							new_argv[0][i] = '\0';
							new_argv[1] = NULL;
							free(line);
							fclose(input);
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
							// use i for store mount flags
							i=0;
							if(!loop_check(&blkdev,"/dev/loop0",&i,&fd_to_close))
							{
								if(!mount(blkdev,NEWROOT,"ext4",i,""))
								{
									// this will silently fail if blkdev is inside /data ( e.g. FS on img file )
									umount("/data");
									if(fd_to_close) // close /dev/loop0 fd
										close(fd_to_close);
									//use blkdev for checking init existence
									snprintf(blkdev,MAX_LINE,"%s%s",root,new_argv[0]);
									if(!access(blkdev,R_OK|X_OK))
									{
										free(blkdev);
										if(!chdir(root) && !chroot(root))
										{
											free(root);
											fclose(log);
											execve(new_argv[0],new_argv,envp);
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
										fprintf(log,"cannot execute \"%s\" - %s\n",blkdev,strerror(errno));
										free(new_argv[0]);
										free(new_argv);
										free(blkdev);
										fclose(log);
										umount(NEWROOT);
									}
								}
								else
								{
									fprintf(log,"unable to mount \"%s\" on %s - %s\n",blkdev,NEWROOT,strerror(errno));
									umount("/data");
									if(fd_to_close)
										close(fd_to_close);
									free(blkdev);
									free(root);
									free(new_argv[0]);
									free(new_argv);
									fclose(log);
								}
							}
							else
							{
								fprintf(log,"loop_check failed on \"%s\" - %s\n",blkdev,strerror(errno));
								umount("/data");
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
					else // this should never happen but i love error checking ;)
					{
						fprintf(log,"malloc - %s\n",strerror(errno));
						if(!line)
							fprintf(log,"failed to alloc \"line\"\n");
						else if(!blkdev)
						{
							free(line);
							fprintf(log,"failed to alloc \"blkdev\"\n");
						}
						else if(!root)
						{
							free(line);
							free(blkdev);
							fprintf(log,"failed to alloc \"root\"\n");
						}
						else if(!new_argv)
						{
							free(line);
							free(blkdev);
							free(root);
							fprintf(log,"failed to alloc \"new_argv\"\n");
						}
						else
						{
							free(line);
							free(blkdev);
							free(root);
							free(new_argv);
							fprintf(log,"failed to alloc \"new_argv[0]\"\n");
						}
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
}
