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

// percentage of screen used by the menu
#define MENU_WIDTH_PERC 50
#define MENU_HEIGHT_PERC 50
// percentage of screen used by the messages
#define MSG_HEIGHT_PERC 25
#define MSG_WIDTH_PERC 100
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

int nc_compute_menu(menu_entry *list);
int nc_init(void);
void nc_destroy(void);
void nc_wait_enter(void);
int nc_get_user_choice(menu_entry *list);