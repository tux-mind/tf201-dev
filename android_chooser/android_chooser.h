#define NEWROOT "/newroot/"
#define NEWROOT_STRLEN 9
#define DATADIR "/newroot/.data/"
#define DATADIR_STRLEN 15
#define LOG "/newroot/android_chooser.log"
#define PERSISTENT_LOG "/newroot/.data/android_chooser.log"
#define BUSYBOX "/bin/busybox"
#define MAX_LINE 255
#define TIMEOUT 5 /* time to wait for external block devices ( USB stick ) */
#define INIT_MAX_ARGS 15 /* maximum number of arguments for the real init */
#define UDEV_PATH "/sbin/ueventd"
#define FAKE_UDEV	"/bin/sleep"
#define TMP_FSTAB	"/fstab.tmp"

#if NEWROOT_STRLEN > MAX_LINE
# error "NEWROOT_STRLEN must be shorter then MAX_LINE"
#endif

// start android init at start for give ADB access
#define ADB
#define DEBUG

#define MDEV_ARGS { "/bin/mdev","-s",NULL }

#define COMMAND_LINE_SIZE 1024
//our option from /proc/cmdline
#define CMDLINE_OPTION "newandroid="
#define CMDLINE_OPTION_LEN 11

#define EXIT_SILENT     	fclose(logfile); \
							free_list(list); \
							fatal(argv,envp); \
							exit(EXIT_FAILURE);
#define EXIT_ERROR(args...)	do{ fprintf(logfile,##args); EXIT_SILENT }while(0)
#define EXIT_ERRNO(format,args...)	EXIT_ERROR(format " - %s\n",##args ,strerror(errno))
//from loop_mount3.c
int try_loop_mount(char **, const char *);
int set_loop(const char *, char *,int *);
//from initrd_mount.c
int try_initrd_mount(char **, const char *);
