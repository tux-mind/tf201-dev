#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define STR "/dev/mmcblk0p8:/:/sbin/init"

int main(int argc, char **argv,char **envp)
{
	char *pos,*line,a[20],b[20],c[20];
	int i;
	line=malloc(200);
	strncpy(line,STR,200);
	for(i=0,pos=line;*pos!=':'&&*pos!='\0';pos++)
		a[i++] = *pos;
	if(*pos==':')
		pos++;
	for(i=0;*pos!=':'&&*pos!='\0';pos++)
		b[i++] = *pos;
	if(*pos==':')
		pos++;
	for(i=0;*pos!='\0';pos++)
		c[i++] = *pos;
	printf("%s - %s - %s\n",a,b,c);
	free(line);
	return 0;
}
