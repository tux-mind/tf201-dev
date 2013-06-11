#ifndef _MENU_H
#define _MENU_H

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
menu_entry *add_entry(menu_entry *, char *, char *,char *, char *, char *);
menu_entry *del_entry(menu_entry *, menu_entry *);
menu_entry *get_item_by_id(menu_entry *, int);
#endif
