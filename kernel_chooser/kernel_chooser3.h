#define NEWROOT "/mnt/"
#define NEWROOT_STRLEN 5
#define BUSYBOX "/bin/busybox"
#define TIMEOUT_BLKDEV 5 /* time to wait for external block devices ( USB stick ) or console */

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
#define DEFAULT_CONFIG_NAME "default" // fallback name for default config if it has no name/description
// the console to use
#define CONSOLE "/dev/tty1"
// maximum length for a boot entry name
#define MAX_NAME 120

// from kexec.c
int k_load(char *,char *,char *);
void k_exec(void);
// from nGUI.c
int nc_compute_menu(menu_entry *list);
int nc_init(void);
void nc_destroy(void);
void nc_wait_enter(void);
int nc_get_user_choice(menu_entry *list);
void nc_print_header(void);
int nc_wait_for_keypress(void);
void nc_destroy_menu(void);
// from nGUI.h
/* our functions return integers
 * i known that a char it's an int value,
 * but user can have an entry ID which have the same value of a char.
 * so i decided to use negative numbers for special entries.
 */
#define MENU_PROMPT			 0
#define MENU_REBOOT			-1
#define MENU_HALT 			-2
#define MENU_RECOVERY		-3
#define MENU_SHELL			-4
#define MENU_DEFAULT		-5
#define MENU_FATAL_ERROR	-6
 