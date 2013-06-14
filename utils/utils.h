//from loop_mount.c
int loop_mount(char *, const char *);
int set_loop(const char *, char *,int *);
//from initrd_mount.c
int initrd_extract(char *, const char *);
int initrd_mount(char *, const char *);
//from zlib.c
char *zlib_decompress_file(const char *, off_t *);
int read_first_bytes_of_archive(char *, char *, int );
//from detect_fs.c
const char *find_filesystem(char *);