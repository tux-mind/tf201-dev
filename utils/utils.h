//from loop_mount.c
int loop_mount(char *, const char *);
int set_loop(const char *, char *,int *);
//from initrd_mount.c
int initrd_extract(char *, const char *);
int initrd_mount(char *, const char *);