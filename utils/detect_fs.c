/* looking at the file(1) sources,
 * you can find the plain-text magic database.
 * as usually, open source rocks ;)
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "detect_fs.h"

typedef signed char s1;
typedef unsigned char u1;
typedef short int s2;
typedef unsigned short int u2;
typedef long int s4;
typedef unsigned long int u4;

u2 get_le_short(void *from)
{
  u1 *p = from;
  return ((u2)(p[1]) << 8) +
    (u2)p[0];
}

u4 get_le_long(void *from)
{
  u1 *p = from;
  return ((u4)(p[3]) << 24) +
    ((u4)(p[2]) << 16) +
    ((u4)(p[1]) << 8) +
    (u4)p[0];
}

extern FILE *logfile;

const char *find_filesystem(char *file)
{
	char buffer[BUFFER_SIZE];
	int fd,len,clustersize;

	if((fd = open(file,O_RDONLY)) < 0)
			return NULL;
	if(read(fd,buffer,BUFFER_SIZE) < 0)
	{
			close(fd);
			return NULL;
	}
	close(fd);
	
	// check for linux ext FS
	if(FS_EXT(buffer))
	{
		if(!EXT_JOURNAL(buffer))
			return "ext2";
		else if (EXT_SMALL_INCOMPAT(buffer) && EXT_SMALL_RO_COMPAT(buffer))
			return "ext3";
		else
			return "ext4";
	}
	// TODO: detect others FS
	errno=EOPNOTSUPP;
	return NULL;
}