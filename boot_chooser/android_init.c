#include <stdio.h>

int main()
{
	FILE *fp = fopen("/dev/kmsg","w");
	fprintf(fp,"Greetings from android init!\n");
	fclose(fp);
	return 0;
}