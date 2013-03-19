#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <curses.h>
#include <menu.h>

#include "menu3.h"
#include "common2.h"

int printed_lines,have_default;

void free_entry(menu_entry *item)
{
	if(item->name)
		free(item->name);
	if(item->blkdev)
		free(item->blkdev);
	if(item->kernel)
		free(item->kernel);
	if(item->cmdline)
		free(item->cmdline);
	if(item->initrd)
		free(item->initrd);
	free(item);
}

void print_in_middle(WINDOW *win, int starty, int startx, int width, char *string, chtype color);

int nc_get_user_choice(menu_entry *list)
{
	ITEM **my_items;
	int c;
	MENU *my_menu;
	WINDOW *my_menu_win;
	int n_choices, i, sizey, sizex;
	menu_entry *current;
	char **local_entries;

	/* Initialize curses */
	initscr();
	start_color();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	init_pair(1, COLOR_RED, COLOR_BLACK);
	init_pair(2, COLOR_CYAN, COLOR_BLACK);

	/* Compute manu size */
	sizey = (LINES * MENU_HEIGHT_PERC)/100;
	sizex = (COLS * MENU_WIDTH_PERC)/100;

	/* Create items */
	for(n_choices=0,current=list;current;current=current->next,n_choices++);
	my_items = (ITEM **)malloc((n_choices+1)*sizeof(ITEM *)); // TODO: error handling
	local_entries = malloc((n_choices+1)*sizeof(char *));
	for(i=0,current=list;current;current=current->next,i++)
	{
		c = strlen(current->name); // TODO TODO TODO TODO TODO TODO
		local_entries[i] = malloc((sizex+1)*sizeof(char));
		memset(local_entries[i],' ',sizex);
		strncpy((local_entries[i] + ((sizex - c)/2)),current->name,c);
		local_entries[i][sizex]='\0';
		my_items[i] = new_item(local_entries[i], NULL); // TODO: padd
	}
	my_items[i] = new_item(NULL,NULL);
	/* Create menu */
	my_menu = new_menu((ITEM **)my_items);
	/* Set menu option not to show the description */
	menu_opts_off(my_menu, O_SHOWDESC);

	/* Create the window to be associated with the menu */
	my_menu_win = newwin( sizey, sizex, (LINES-sizey)/2, (COLS-sizex)/2);
	keypad(my_menu_win, TRUE);

	/* Set main window and sub window */
	set_menu_win(my_menu, my_menu_win);
	set_menu_sub(my_menu, derwin(my_menu_win, sizey-3, sizex -2, 3, 1));
	//set_menu_format(my_menu, 5, 1); // this set maximum Y
	set_menu_format(my_menu, sizey-3,1);

	/* Set menu mark to the string " * " */
	set_menu_mark(my_menu, NULL);
	/* Print a border around the main window and print a title */
	box(my_menu_win, 0, 0);
	print_in_middle(my_menu_win, 1, 0, sizex, "kernel_chooser", COLOR_PAIR(1));
	mvwaddch(my_menu_win, 2, 0, ACS_LTEE);
	mvwhline(my_menu_win, 2, 1, ACS_HLINE, sizex-2);
	mvwaddch(my_menu_win, 2, sizex-1, ACS_RTEE);
	/* Post the menu */
	post_menu(my_menu);
	wrefresh(my_menu_win);

	attron(COLOR_PAIR(2));
	mvprintw(LINES - 2, 0, "Use PageUp and PageDown to scoll down or up a page of items");
	mvprintw(LINES - 1, 0, "Arrow Keys to navigate (Q to Exit)");
	attroff(COLOR_PAIR(2));
	refresh();

	while((c = wgetch(my_menu_win)) != 'q')
	{
		switch(c)
		{
			case KEY_DOWN:
				menu_driver(my_menu, REQ_DOWN_ITEM);
				break;
			case KEY_UP:
				menu_driver(my_menu, REQ_UP_ITEM);
				break;
			case KEY_NPAGE:
				menu_driver(my_menu, REQ_SCR_DPAGE);
				break;
			case KEY_PPAGE:
				menu_driver(my_menu, REQ_SCR_UPAGE);
				break;
		}
		wrefresh(my_menu_win);
	}

	/* Unpost and free all the memory taken up */
	//TODO: FREEEEEEEEEE
	unpost_menu(my_menu);
	free_menu(my_menu);
	for(i = 0; i < n_choices; ++i)
					free_item(my_items[i]);
	free(my_items);
	endwin();
	return MENU_REBOOT_NUM;
}

void print_in_middle(WINDOW *win, int starty, int startx, int width, char *string, chtype color)
{
	int length, x, y;
	float temp;

	if(win == NULL)
		win = stdscr;
	getyx(win, y, x);
	if(startx != 0)
		x = startx;
	if(starty != 0)
		y = starty;
	if(width == 0)
		width = 80;

	length = strlen(string);
	temp = (width - length)/ 2;
	x = startx + (int)temp;
	wattron(win, color);
	mvwprintw(win, y, x, "%s", string);
	wattroff(win, color);
	refresh();
}

void free_list(menu_entry *list)
{
	menu_entry *current;
	for(current=list;current;current=current->next)
		free_entry(current);
}

/** add an etry in the list
 * entries have an ID which is univoke for everyone.
 * this id starts from 1, since we decided that end-users can prefer this.
 * @list: the main list
 * @_name: the name pointer.
 * @_blkdev: the block device pointer.
 * @_kernel: the kernel path pointer.
 * @_cmdline: the cmdline pointer.
 * @_initrd: the initrd pointer.
 */
menu_entry *add_entry(menu_entry *list, char *_name, char *_blkdev,char *_kernel, char *_cmdline, char *_initrd)
{
	menu_entry *item;
	static unsigned short id = 1;

	if(!list)
	{
		list = item = malloc(sizeof(menu_entry));
		if(!list)
			return NULL;
	}
	else
	{
		for(item=list;item->next;item=item->next);
		item = item->next = malloc(sizeof(menu_entry));
		if(!item)
			return NULL;
	}
	item->id = id++;
	item->name = _name;
	item->blkdev = _blkdev;
	item->kernel = _kernel;
	item->cmdline = _cmdline;
	item->initrd = _initrd;
	item->next = NULL;
	return list;
}

menu_entry *del_entry(menu_entry *list, menu_entry *item)
{
	menu_entry *prev,*current;

	if(item!=list)
	{
		for(current=list;current && current != item;current=current->next)
			prev=current;
		prev->next = current->next;
	}
	else
	{
		current = list;
		list = list->next;
	}
	free_entry(current);
	return list;
}

void print_menu(menu_entry *list)
{
	menu_entry *current;
	// clear screen
	for(;printed_lines;printed_lines--)
		printf("\033[A\033[2K"); // go UP and CLEAR line ( see VT100 reference )
	rewind(stdout);
	ftruncate(1,0);
	// print entries
	if(have_default)
	{
		printf("%c) boot the default config\n",MENU_DEFAULT);
		printed_lines++;
	}
	printf("%c) reboot\n",MENU_REBOOT);
	printf("%c) poweroff\n",MENU_HALT);
	printf("%c) reboot recovery\n",MENU_RECOVERY);
#ifdef SHELL
	printf("%c) emergency shell\n",MENU_SHELL);
	printed_lines++;
#endif
	printf("   ------------------\n");
	for(printed_lines+=4,current=list;current;current=current->next,printed_lines++)
		printf("%u) %s\n",current->id,current->name);
}

void clear_screen(void)
{
	for(;printed_lines;printed_lines--)
		printf("\033[A\033[2K"); // see print_menu for info
	rewind(stdout);
	ftruncate(1,0);
}

menu_entry *get_item_by_id(menu_entry *list, int id)
{
	for(;list;list=list->next)
		if(list->id == id)
			return list;
	return NULL;
}
