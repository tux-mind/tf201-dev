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

void usage(char *argv0, int exit_status,FILE *stream)
{
	fprintf(stream,"control asusdec-elantech touchpad\nUsage: %s <on|off>\n",argv0);
	exit(exit_status);
}

int main(int argc,char **argv)
{
	int fd,arg;

	if(argc != 2)
		usage(argv[0],EXIT_SUCCESS,stdout);

	if(!strncmp("on",argv[1],3))
		arg = ASUSDEC_TP_ON;
	else if(!strncmp("off",argv[1],4))
		arg = ASUSDEC_TP_OFF;
	else
		usage(argv[0],EXIT_FAILURE,stderr);
	if((fd = open("/dev/asusdec",O_WRONLY)) == -1)
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