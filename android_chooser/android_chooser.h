#define NEWROOT "/newroot/"
#define NEWROOT_STRLEN 9
#define DATADIR "/newroot/.data/"
#define DATADIR_STRLEN 15
#define LOG "/newroot/android_chooser.log"
#define BUSYBOX "/bin/busybox"
#define MAX_LINE 255
#define TIMEOUT 5 /* time to wait for external block devices ( USB stick ) */
#define INIT_MAX_ARGS 15 /* maximum number of arguments for the real init */

#if NEWROOT_STRLEN > MAX_LINE
# error "NEWROOT_STRLEN must be shorter then MAX_LINE"
#endif

// start android init at start for give ADB access
#define ADB

#define MDEV_ARGS { "/bin/mdev","-s",NULL }

#define COMMAND_LINE_SIZE 1024
//our option from /proc/cmdline
#define CMDLINE_OPTION "newandroid="
#define CMDLINE_OPTION_LEN 11

#define EXIT_SILENT     fclose(log); \
						free_list(list); \
                        fatal(argv,envp); \
                        exit(EXIT_FAILURE);
#define EXIT_ERROR(err) fprintf(log, err " - %s\n", strerror(errno)); \
                        EXIT_SILENT

//from loop_mount3.c
int try_loop_mount(char **, const char *);
//from initrd_mount.c
int try_initrd_mount(char **, const char *);
