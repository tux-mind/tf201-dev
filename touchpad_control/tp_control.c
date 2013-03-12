#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define ASUSDEC_TP_CONTROL 0x8004F405
#define ASUSDEC_TP_ON 1
#define ASUSDEC_TP_OFF 0
#define ASUSDEC_TP_TOOGLE	2
#define IOCTL_DEVICE "/dev/asusdec"

int do_ioctl_call(int arg)
{
	int fd;

	if((fd = open(IOCTL_DEVICE,O_WRONLY)) == -1)
	{
		perror("open()");
		return EXIT_FAILURE;
	}
	if(ioctl(fd,ASUSDEC_TP_CONTROL,arg) == -1)
	{
		close(fd);
		perror("ioctl()");
		return EXIT_FAILURE;
	}
	close(fd);
	return EXIT_SUCCESS;
}

void usage(char *argv0, int exit_status,FILE *stream)
{
	fprintf(stream,"control asusdec-elantech touchpad\nUsage: %s [on|off]\n\nif no argument is given try to toogle the current state ( require v2 kernel ).\n",argv0);
	exit(exit_status);
}

int main(int argc,char **argv)
{
	int arg;

	if(argc == 1) // no option given, try to toogle status
		return do_ioctl_call(ASUSDEC_TP_TOOGLE);
	else if(argc > 2)
		usage(argv[0],EXIT_FAILURE,stderr);
	else if(!strncmp("--help",argv[1],7) || !strncmp("-h",argv[1],3))
		usage(argv[0],EXIT_SUCCESS,stdout);
	else if(!strncmp("on",argv[1],3))
		return do_ioctl_call(ASUSDEC_TP_ON);
	else if(!strncmp("off",argv[1],4))
		return do_ioctl_call(ASUSDEC_TP_OFF);
	else
		usage(argv[0],EXIT_FAILURE,stderr);
	return EXIT_FAILURE;
}