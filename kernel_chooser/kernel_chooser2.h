#define NEWROOT "/mnt/"
#define NEWROOT_STRLEN 5
#define BUSYBOX "/bin/busybox"
#define TIMEOUT 5 /* time to wait for external block devices ( USB stick ) or console */
#define TIMEOUT_BOOT 10 /* time to wait for the user to press a key */

#if NEWROOT_STRLEN > MAX_LINE
 #error "NEWROOT_STRLEN must be shorter then MAX_LINE"
#endif

#define MDEV_ARGS { "/bin/mdev","-s",NULL }
#define SHELL_ARGS { "/bin/sh","-s", NULL }

// the device containing DATA_DIR
#define DATA_DEV "/dev/mmcblk0p8"
// the directory contains all configs
#define DATA_DIR "/data/.kernel.d/"
#define DATA_DIR_STRLEN 16
// the name of the file where we read the default boot options
#define DEFAULT_CONFIG "/data/.kernel"
// the console to use
#define CONSOLE "/dev/tty1"
// maximum length for a boot entry name
#define MAX_NAME 120

#define HEADER 	"kernel_chooser - version 1\n"\
				"say THANKS to the 4 penguins!\n"\
				"Open Source rocks! - tux_mind <massimo.dragano@gmail.com>\n"\
				"                   - smasher816 <smasher816@gmail.com>\n\n"

// from kexec.c
int k_load(char *,char *,char *);
void k_exec(void);
