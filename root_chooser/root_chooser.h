#define NEWROOT "/newroot/"
#define NEWROOT_STRLEN 9
#define LOG "/newroot/root_chooser.log"
#define BUSYBOX "/bin/busybox"
#define MAX_LINE 255
#define TIMEOUT 5 /* time to wait for external block devices ( USB stick ) */
#define INIT_MAX_ARGS 15 /* maximum number of arguments for the real init */

#if NEWROOT_STRLEN > MAX_LINE
# error "NEWROOT_STRLEN must be shorter then MAX_LINE"
#endif

// start android init at start for give ADB access
//#define ADB

#define MDEV_ARGS { "/bin/mdev","-s",NULL }

//where we looking for .root file
#define DATA_DEV "/dev/mmcblk0p8"
//the name of the file where we read the boot options
#define ROOT_FILE "/data/.root"
//the name of the temporary file where we read the boot options
#define ROOT_TMP_FILE "/data/.root.tmp"
#define COMMAND_LINE_SIZE 1024
//our option from /proc/cmdline
#define CMDLINE_OPTION "newroot="
#define CMDLINE_OPTION_LEN 8
