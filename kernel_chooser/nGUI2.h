/* our functions return integers
 * i known that a char it's an int value,
 * but user can have an entry ID which have the same value of a char.
 * so i decided to use negative numbers for special entries.
 */
#define MENU_REBOOT		-1
#define MENU_HALT 		-2
#define MENU_RECOVERY	-3
#define MENU_SHELL		-4
#define MENU_DEFAULT	-5
#define MENU_FATAL_ERROR	-6

// colors
#define COLOR_DEFAULT 0
#define COLOR_LOG_DEBUG 1
#define COLOR_LOG_WARN 2
#define COLOR_LOG_ERROR 3
#define COLOR_MENU_BORDER 4
#define COLOR_MENU_TEXT 5
#define COLOR_MENU_TITLE 6

// percentage of screen used by the menu
#define MENU_WIDTH_PERC 80
#define MENU_HEIGHT_PERC 65
// percentage of screen used by the messages
#define MSG_HEIGHT_PERC 25
#define MSG_WIDTH_PERC 100

#define WAIT_MESSAGE "Automatic boot in %2d..."
#define TIMEOUT_BOOT 10 /* time to wait for the user to press a key */

#define HEADER 	\
{\
	"                      kernel_chooser - version 2                           ",\
	"                  Open Source rocks! - tux_mind <massimo.dragano@gmail.com>",\
	"                                     - smasher816 <smasher816@gmail.com>   ",\
	NULL\
}

#define PROMPT "choose an option"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

int nc_compute_menu(menu_entry *list);
int nc_init(void);
void nc_destroy(void);
void nc_destroy_menu(void);
void nc_wait_enter(void);
int nc_get_user_choice(menu_entry *list);
void nc_print_header(void);
int nc_wait_for_keypress(void);