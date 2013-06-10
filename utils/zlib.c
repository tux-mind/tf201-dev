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
#include <ctype.h>
#include <zlib.h>

char *zlib_decompress_file(const char *filename, off_t *r_size)
{
	gzFile fp;
	//int errnum;
	//const char *msg;
	char *buf,*tmp;
	off_t size, allocated;
	ssize_t result;

	if (!filename) {
		*r_size = 0;
		return 0;
	}
	fp = gzopen(filename, "rb");
	if (fp == 0) {
		/*msg = gzerror(fp, &errnum);
		if (errnum == Z_ERRNO) {
			msg = strerror(errno);
		}
		ERROR("cannot open \"%s\" - %s\n", filename, msg);*/
		return NULL;
	}
	size = 0;
	allocated = 65536;
	buf = malloc(allocated);
	if(!buf)
	{
		//FATAL("malloc - %s\n",strerror(errno));
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
				//FATAL("realloc - %s\n",strerror(errno));
				return NULL;
			}
			buf = tmp;
		}
		result = gzread(fp, buf + size, allocated - size);
		if (result < 0) {
			if ((errno == EINTR) || (errno == EAGAIN))
				continue;
			/*
			msg = gzerror(fp, &errnum);
			if (errnum == Z_ERRNO) {
				msg = strerror(errno);
			}
			ERROR ("read on %s of %ld bytes failed: %s\n",
				filename, (allocated - size) + 0UL, msg);*/
			free(buf);
			gzclose(fp);
			return NULL;
		}
		size += result;
	} while(result > 0);
	result = gzclose(fp);
	if (result != Z_OK) {
		/*
		msg = gzerror(fp, &errnum);
		if (errnum == Z_ERRNO) {
			msg = strerror(errno);
		}
		ERROR ("close of %s failed: %s\n", filename, msg);*/
		free(buf);
		return NULL;
	}
	*r_size =  size;
	return buf;
}
