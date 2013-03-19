#ifndef _MENU_H
#define _MENU_H

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
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

typedef struct _menu_entry
{
	unsigned int id;
	char 	*name,
				*blkdev,
				*kernel,
				*cmdline,
				*initrd;
	struct _menu_entry *next;
} menu_entry;

void free_entry(menu_entry *);
void free_list(menu_entry *);
//void print_menu(menu_entry *);
void clear_screen(void);
int nc_get_user_choice(menu_entry *);
menu_entry *add_entry(menu_entry *, char *, char *,char *, char *, char *);
menu_entry *del_entry(menu_entry *, menu_entry *);
menu_entry *get_item_by_id(menu_entry *, int);
#endif