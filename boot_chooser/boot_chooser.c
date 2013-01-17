/*
 * TODO:		read wanted boot option from /data, not from volume keys.
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

#define LOG "/android/boot_chooser.log"
#define INTERRUPTS "/proc/interrupts"
#define CMDLINE "/proc/cmdline"
#define ANDROID_ROOT "/android"
#define LINUX_ROOT "/linux"
#define BUSYBOX "/bin/busybox"
#define MAX_LINE 255
#define TIMEOUT 15 /* timeout in secs */
#define DEBUG

#define LINUX_INIT_ARGS { "/sbin/init", NULL }
#define ANDROID_INIT_ARGS { "/init",NULL }
#define MDEV_ARGS { "/bin/mdev","-s",NULL }

//provide linux root partition here, or will be read from kernel command line
#define ROOT "/dev/mmcblk0p8"

//fatal error occourred, boot up android
void fatal(char **argv,char **envp)
{
#ifndef DEBUG
	chdir(ANDROID_ROOT);
#endif
	chroot(ANDROID_ROOT);
	execve("/init",argv,envp);
}

void close_log(FILE *log)
{
	int writed;
	//store the amount of writed bytes
	writed = ftell(log);
	fclose(log);
	//if we don't write anything remove the log file.
	if(!writed)
		unlink(LOG);
}

int main(int argc, char **argv, char **envp)
{
	FILE *log;
	char 	*line, // where we place the readed line
				*root, // the partition for booting up linux
				*lnx_argv[] = LINUX_INIT_ARGS, // args for the linux init
				*and_argv[] = ANDROID_INIT_ARGS; // args for android init
	int i, // used for loops
			fd, // input file descriptor
			readed, // number of readed bytes
			volup_count, // the number of KEY_VOLUP interrupts
			voldown_count; // the number of KEY_VOLUMEDOWN interrupts
	time_t start_time;
	pid_t pid;

	if((log = fopen(LOG,"w")) == NULL)
		fatal(argv,envp);

	//mount proc
	if(mount("proc","/proc","proc",MS_RELATIME,""))
	{
		fprintf(log,"%s\n",strerror(errno));
		fclose(log);
		fatal(argv,envp);
	}
	if((line = malloc(MAX_LINE)) == NULL)
	{
		fprintf(log,"%s\n",strerror(errno));
		fclose(log);
		umount("/proc");
		fatal(argv,envp);
	}
#ifndef ROOT
	//read root from kernel cmdline
	if((root = malloc(30)) == NULL)
	{
		fprintf(log,"%s\n",strerror(errno));
		fclose(log);
		free(line);
		umount("/proc");
		fatal(argv,envp);
	}
	root[0] = '\0';
	if((fd = open(CMDLINE,O_RDONLY)) >= 0)
	{
		do
		{
			for(i=0;i<MAX_LINE && ((readed = read(fd,&line[i],MAX_LINE)) == 1);i++)
				if(line[i] == ' ') // we have an option
				{
					line[i] = '\0'; // terminate string
					if(!strncmp(line,"root=",5))
					{
						strncpy(root,line+5,29);
						root[29] = '\0';
						break;
					}
				}
		}
		while(readed > 0);
		close(fd);
		if(readed < 0) // ERROR
		{
			fprintf(log,"reading \"%s\" - %s\n",CMDLINE,strerror(errno));
			fclose(log);
			free(line);
			free(root);
			umount("/proc");
			fatal(argv,envp);
		}
	}
	else
	{
		fprintf(log,"%s\n",strerror(errno));
		fclose(log);
		free(line);
		free(root);
		umount("/proc");
		fatal(argv,envp);
	}
	if(root[0] == '\0')
	{
		fwrite("no \"root=/dev/XXX\" from CMDLINE\n",32,1,log);
		fclose(log);
		free(line);
		free(root);
		umount("/proc");
		fatal(argv,envp);
	}
#else
	root = ROOT;
#endif
	voldown_count = volup_count = 0;
	time(&start_time);
	if((fd = open(INTERRUPTS,O_RDONLY)) >= 0)
	{
		while(voldown_count == 0 && volup_count == 0 && (time(NULL) - start_time) < TIMEOUT)
		{
			for(i = 0;i < MAX_LINE && ((readed = read(fd,&line[i],1)) == 1);i++)
			{
				if(line[i] == '\n') // we have a line
				{
					if(strstr(line,"KEY_VOLUMEUP"))
						sscanf(line,"%*s%d",&volup_count);
					else if(strstr(line,"KEY_VOLUMEDOWN"))
						sscanf(line,"%*s%d",&voldown_count);
					i=-1; // read next line
				}
			}
			if(readed < 0) // ERROR
			{
				fprintf(log,"reading \"%s\" - %s\n",INTERRUPTS,strerror(errno));
				fclose(log);
				free(line);
#ifndef ROOT
				free(root);
#endif
				close(fd);
				umount("/proc");
				fatal(argv,envp);
			}
			lseek(fd,0L,SEEK_SET);
		}
		close(fd);
	}
	else
	{
		fprintf(log,"opening \"%s\" - %s\n",INTERRUPTS,strerror(errno));
		fclose(log);
		umount("/proc");
		free(line);
#ifndef ROOT
		free(root);
#endif
		fatal(argv,envp);
	}

	free(line);
	if(voldown_count == 0 && volup_count == 0) // something bad
	{
		fwrite("VOLUP/VOLDOWN not pressed\n",26,1,log);
		umount("/proc");
		fclose(log);
#ifndef ROOT
		free(root);
#endif
		fatal(argv,envp);
	}
	if(voldown_count)
	{
#ifndef ROOT
		free(root);
#endif
		close_log(log);
		umount("/proc");
#ifndef DEBUG
		chdir(ANDROID_ROOT);
#endif
		chroot(ANDROID_ROOT);
		execve(and_argv[0],and_argv,envp);
	}
	else if(volup_count)
	{
		if(mount("sysfs","/sys","sysfs",MS_RELATIME,""))
		{
			fwrite("unable to mount /sys\n",21,1,log);
			fclose(log);
			umount("/proc");
			fatal(argv,envp);
		}
		//start mdev
		if(!(pid = fork()))
		{
			char *mdev_argv[] = MDEV_ARGS;
			execve("/bin/busybox",mdev_argv,envp);
			return EXIT_FAILURE;
		}
		waitpid(pid,&i,0);
		umount("/sys");
		//mount root partition into LINUX_ROOT
		if(mount(root,LINUX_ROOT,"ext4",0,""))
		{
			fprintf(log,"unable to mount %s on %s\n",root,LINUX_ROOT);
			fclose(log);
#ifndef ROOT
			free(root);
#endif
			umount("/proc");
			fatal(argv,envp);
		}
#ifndef ROOT
		free(root);
#endif
#ifdef DEBUG
		if(!fork())
			fatal(argv,envp);
#endif
		close_log(log);
		chdir(LINUX_ROOT);
		chroot(LINUX_ROOT);
		execve(lnx_argv[0],lnx_argv,envp);
	}

	return EXIT_FAILURE; // just for make gcc don't fire up warnings
}
