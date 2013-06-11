//#define CPIO_ARGV { "/bin/cpio", "-i", NULL } // this is a work around....we have to find another way..
#define CPIO_ARGV { "/.android_chooser/bin/cpio", "-i", NULL }
// form zlib.c
char *zlib_decompress_file(const char *, unsigned long *);
