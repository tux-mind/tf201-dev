/* Part of this code has been taken from 
 * disktype - http://disktype.sourceforge.net
 * thanks to chrisp@users.sourceforge.net
 * as usually, open source rocks ;)
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
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

const char *find_filesystem(char *file)
{
	char buffer[BUFFER_SIZE];
	int fd,len,clustersize;

	if((fd = open(file,O_RDONLY)) < 0)
			return NULL;
	if(read(fd,buffer,1024) < 0)
	{
			close(fd);
			return NULL;
	}
	close(fd);
	
	// check for linux ext FS
	
	len = strlen(FS_EXT_MAGIC);
	if(!strncmp(buffer + FS_EXT_OFFSET,FS_EXT_MAGIC,len))
	{
		if(!(get_le_long(buffer + EXT3_TEST1_OFF) & EXT3_TEST1_VAL) &&
					!(get_le_long(buffer + EXT3_TEST2_OFF) & EXT3_TEST2_VAL))
			return "ext2";
		else if (	get_le_long(buffer + EXT4_TEST1_OFF) & EXT4_TEST1_VAL ||
						get_le_long(buffer + EXT4_TEST2_OFF) & EXT4_TEST2_VAL || 
						get_le_short(buffer + EXT4_TEST3_OFF) >= EXT4_TEST3_VAL)
			return "ext4";
		else
			return "ext3";
	}
	
	// check for DOS FAT FS
	
	len = get_le_short(buffer+FAT_SECTORSIZE_OFF);
	clustersize = buffer[FAT_CLUSTERSIZE_OFF];
	if(	(len==512 || len==1024 || len==2048 || len==4096 ) &&
		(clustersize != 0 && !(clustersize & (clustersize - 1))))
	{
		if(memcmp(buffer+NTFS_TEST_OFF,NTFS_TEST_VAL,strlen(NTFS_TEST_VAL)))
			return "vfat";
		else
			return "ntfs";
	}
	// TODO: detect others FS
	return NULL;
}