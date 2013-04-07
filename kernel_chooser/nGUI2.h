/* our functions return integers
 * i known that a char it's an int value,
 * but user can have an entry ID which have the same value of a char.
 * so i decided to use negative numbers for special entries.
 */
#define MENU_REBOOT			-1
#define MENU_HALT 			-2
#define MENU_RECOVERY		-3
#define MENU_SHELL			-4
#define MENU_SCREENSHOT		-5
#define MENU_DEFAULT		-6
#define MENU_FATAL_ERROR	-7

// menu screens
#define MENU_COUNT 2
#define MENU_MAIN  0
#define MENU_POWER 1

// percentage of screen used by the menu
#define MENU_WIDTH_PERC 80
#define MENU_HEIGHT_PERC 65
// percentage of screen used by the messages
#define MSG_HEIGHT_PERC 15
#define MSG_WIDTH_PERC 100

#define WAIT_MESSAGE "Automatic boot in %2d..."
#define TIMEOUT_BOOT 10 /* time to wait for the user to press a key */

#define HEADER_LEFT "kernel_chooser v3"
#define HEADER_RIGHT "github.com/tux-mind/tf201-dev/kernel_chooser"

#define HELP_KEY 'h'
#define HELP_MESSAGE "Press [h] for help"
#define HELP_PAGE 	\
{\
	"kernel_chooser version 3",\
	"",\
	"Created by - tux_mind <massimo.dragano@gmail.com>",\
	"           - smasher816 <smasher816@gmail.com>",\
	"",\
	"Open Source rocks!",\
	"For more information please visit https://github.com/tux-mind/tf201-dev",\
	"",\
	"Controls:",\
	"    Up Arrow / Volume Up     - Go up an item",\
	"    Down Arrow / Volume Down - Go down an item",\
	"    Enter / Power            - Choose the selected item",\
	"    H                        - Open this help screen",\
	"    R                        - Toggle the reboot menu",\
	"",\
	"Configuration:",\
	"    The default choice is /data/.kernel",\
	"    All menu items are added in the /data/.kernel.d/ directory, with the following format",\
	"",\
	"    DESCRIPTION",\
	"    blkdev:kernel:initrd",\
	"    CMDLINE",\
	"",\
	"    For descriptions of the items and example configs, please check the github webpage",\
	"",\
	"",\
	"",\
	NULL\
}

#define FATAL_TITLE "An unrecoverable error occured!"
#define PRESS_ENTER "press <ENTER> to continue..."
#define PROMPT "choose an option"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

int nc_compute_menu(menu_entry *list);
int nc_init(void);
void nc_destroy(void);
void nc_destroy_menu(void);
void nc_save();
void nc_load();
void nc_wait_enter(void);
int nc_get_user_choice();
void nc_print_header(void);
int nc_wait_for_keypress(void);