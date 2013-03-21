#include <curses.h>
#include <menu.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "common2.h"
#include "menu3.h"
#include "nGUI.h"

int	menu_sizex, // size fo the menu_window ( getmaxyx does not work )
		msg_sizey, // same as above
		entries_count; // count of created entries ( useful for error handling )
ITEM **items; // ncurses menu items
MENU *menu; // ncurses menu
WINDOW 	*menu_window, // ncurses menu window
				*messages_win; // ncurses messages window
char 	**local_entries; // our padded copy of the items names

struct _default_entries
{
	const char *name;
	const int num;
};

const struct _default_entries default_entries[] =
{
	{
		.name = "reboot",
		.num = MENU_REBOOT
	},
	{
		.name = "shutdown",
		.num = MENU_HALT
	},
	{
		.name = "recovery",
		.num = MENU_RECOVERY
	}
#ifdef SHELL
	,
	{
		.name = "emergency shell",
		.num = MENU_SHELL
	}
#endif
};

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

/** copy src string to *dst, padding equally from right and left
 * @dest: destination char *
 * @src: source char *
 * @sizex: size of the padded string
*/
int copy_with_padd(char **dest, int sizex, char *src)
{
	int len;

	len = strlen(src);
	*dest = malloc((sizex+1)*sizeof(char));
	if(!*dest)
		return -1;
	memset(*dest,' ',sizex);
	strncpy((*dest + ((sizex - len)/2)),src,len);
	*((*dest)+sizex)='\0';
	return 0;
}

int nc_init(void)
{
	int sizex;

	/* Initialize variables to free in case of errors */
	menu = NULL;
	messages_win = menu_window = NULL;
	items = NULL;
	local_entries = NULL;

	/* Initialize curses */
	initscr(); // TODO: error checking
	start_color();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	init_pair(1, COLOR_RED, COLOR_BLACK);
	init_pair(2, COLOR_CYAN, COLOR_BLACK);

	/* Create messages window */
	msg_sizey = (LINES * MSG_HEIGHT_PERC)/100;
	sizex = (COLS * MSG_WIDTH_PERC)/100;
	messages_win = newwin(msg_sizey-2,sizex-2,(LINES-msg_sizey)+1,1);
	scrollok(messages_win,TRUE);

	mvaddch(LINES-msg_sizey,0,ACS_ULCORNER);
	mvhline(LINES-msg_sizey,1,ACS_HLINE,sizex-2);
	mvaddch(LINES-msg_sizey,sizex-1,ACS_URCORNER);
	mvaddch(LINES-1,0,ACS_LLCORNER);
	mvhline(LINES-1,1,ACS_HLINE,sizex-2);
	mvaddch(LINES-1,sizex-1,ACS_LRCORNER);
	mvvline(LINES-msg_sizey+1,0,ACS_VLINE,msg_sizey-2);
	mvvline(LINES-msg_sizey+1,sizex-1,ACS_VLINE,msg_sizey-2);
	wrefresh(messages_win);
	refresh();
	return 0;
}

void nc_destroy(void)
{
	unpost_menu(menu);
	free_menu(menu);
	delwin(menu_window);
	delwin(messages_win);
	if(items[entries_count]) // items are NULL-terminated
		free_item(items[entries_count]);
	while(entries_count--)
	{
		free(local_entries[entries_count]);
		free_item(items[entries_count]);
	}
	free(items);
	free(local_entries);
	clear();
	endwin();
}

int nc_compute_menu(menu_entry *list)
{
	int n_choices, sizey, default_count;
	menu_entry *current;

	/* Compute menu size */
	sizey = (LINES * MENU_HEIGHT_PERC)/100;
	menu_sizex = (COLS * MENU_WIDTH_PERC)/100;

	/* Create items */
	// count entries
	for(n_choices=0,current=list;current;current=current->next,n_choices++);
	// count the default ones
	default_count = ARRAY_SIZE(default_entries);
	// sum
	n_choices+= default_count;
	items = (ITEM **)malloc((n_choices+1)*sizeof(ITEM *));
	local_entries = malloc((n_choices+1)*sizeof(char *));
	if(!items || !local_entries)
		goto error;
	// create default entries
	for(entries_count=0;entries_count<default_count;entries_count++)
		if(copy_with_padd(local_entries+entries_count,menu_sizex,(char*)default_entries[entries_count].name))
			goto error;
		else
		{
			// HACK: store src pointer in item description
			items[entries_count] = new_item(local_entries[entries_count], default_entries[entries_count].name);
			if(!items[entries_count])
			{
				free(local_entries[entries_count]);
				goto error;
			}
		}
	for(current=list;current;current=current->next,entries_count++)
		if(copy_with_padd(local_entries+entries_count,menu_sizex,current->name))
			goto error;
		else
		{
			// HACK: store src pointer in item description
			items[entries_count] = new_item(local_entries[entries_count], current->name);
			if(!items[entries_count])
			{
				free(local_entries[entries_count]);
				goto error;
			}
		}
	items[entries_count] = new_item(NULL,NULL);
	/* Create menu */
	menu = new_menu((ITEM **)items);
	if(!menu)
		goto error;
	/* Set menu option not to show the description */
	menu_opts_off(menu, O_SHOWDESC);

	/* Create the window to be associated with the menu */
	menu_window = newwin( sizey, menu_sizex, (LINES-sizey)/2, (COLS-menu_sizex)/2);
	if(!menu_window)
		goto error;

	keypad(menu_window, TRUE);

	/* Set main window and sub window */
	if(set_menu_win(menu, menu_window)==ERR)
		goto error;
	if(set_menu_sub(menu, derwin(menu_window, sizey-3, menu_sizex -2, 3, 1)) != E_OK)
		goto error;
	// set menu size ( maxY = sizey -3, columns = 1 )
	if(set_menu_format(menu, sizey-3,1) != E_OK)
		goto error;

	/* Set menu mark to the string " * " */
	if(set_menu_mark(menu, NULL)!= E_OK)
		goto error;
	return 0;
	error:
	nc_destroy();
	return -1;
}

int nc_get_user_choice(menu_entry *list)
{
	int c,selected_id,n_choices,default_count;
	menu_entry *current;
	char *current_name;

	default_count = ARRAY_SIZE(default_entries);

	/* Print a border around the main window and print a title */
	box(menu_window, 0, 0); // TODO: no error check here, manpage is not clear
	print_in_middle(menu_window, 1, 0, menu_sizex, "kernel_chooser", COLOR_PAIR(1));
	if(mvwaddch(menu_window, 2, 0, ACS_LTEE)==ERR)
		goto error;
	mvwhline(menu_window, 2, 1, ACS_HLINE, menu_sizex-2); // TODO: same as box()
	if(mvwaddch(menu_window, 2, menu_sizex-1, ACS_RTEE)==ERR)
		goto error;
	/* Post the menu */
	if(post_menu(menu)!=E_OK)
		goto error;
	wrefresh(menu_window);
	wrefresh(messages_win);

	while((c = wgetch(menu_window)) != 10)
	{
		switch(c)
		{
			case 278:
			case KEY_DOWN:
				menu_driver(menu, REQ_DOWN_ITEM);
				break;
			//case KEY_VOLUMEUP: TODO
			case KEY_UP:
				menu_driver(menu, REQ_UP_ITEM);
				break;
			case KEY_NPAGE:
				menu_driver(menu, REQ_SCR_DPAGE);
				break;
			case KEY_PPAGE:
				menu_driver(menu, REQ_SCR_UPAGE);
				break;
		}
		wrefresh(menu_window);
	}
	current_name = (char*)item_description(current_item(menu));
	for(current=list;current && current->name != current_name;current=current->next);
	if(current)
		selected_id = current->id;
	// HACK: use n_choices as temporary variable
	for(n_choices=0;!selected_id && n_choices<default_count;n_choices++)
		if(default_entries[n_choices].name == current_name)
			selected_id = default_entries[n_choices].num;
	if(selected_id)
		return selected_id;
	error:
	return MENU_FATAL_ERROR;
}

void nc_push_message(char *fmt,...)
{
	va_list ap;

	if(!messages_win)
		return;

	va_start(ap,fmt);
	vwprintw(messages_win,fmt,ap);
	wrefresh(messages_win);
	va_end(ap);
	refresh();
}

void nc_wait_enter(void)
{
	while(getch() != 10)
		usleep(100); //TODO: use usec
}

void nc_wait_for_keypress(void)
{
	getch();
}
