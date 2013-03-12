#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

char *zlib_decompress_file(const char *filename, unsigned long *r_size)
{
	gzFile fp;
	char *buf,*tmp;
	unsigned long size, allocated;
	ssize_t result;

	if (!filename) {
		*r_size = 0;
		return 0;
	}
	fp = gzopen(filename, "rb");
	if (fp == 0)
		return NULL;
	size = 0;
	allocated = 65536;
	buf = malloc(allocated);
	if(!buf)
	{
		gzclose(fp);
		return NULL;
	}
	do {
		if (size == allocated) {
			allocated <<= 1;
			tmp = realloc(buf, allocated);
			if(!tmp)
			{
				free(buf);
				gzclose(fp);
				return NULL;
			}
			buf = tmp;
		}
		result = gzread(fp, buf + size, allocated - size);
		if (result < 0)
		{
			if ((errno == EINTR) || (errno == EAGAIN))
				continue;
			free(buf);
			gzclose(fp);
			return NULL;
		}
		size += result;
	} while(result > 0);
	result = gzclose(fp);
	if (result != Z_OK)
	{
		free(buf);
		return NULL;
	}
	*r_size =  size;
	return buf;
}
