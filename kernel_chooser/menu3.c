#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>

#include "menu2.h"
#include "common2.h"

int printed_lines,have_default,startx,menu_width;
char *line;

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

void free_menu(menu_entry *list)
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
	printf("%s\n",line);
	for(printed_lines+=4,current=list;current;current=current->next,printed_lines++)
		printf("%u) %s\n",current->id,current->name);
}

void get_term_size(int *x, int*y)
{
	struct winsize term;

	ioctl(STDOUT_FILENO, TIOCGWINSZ,&term);
	*x = term.ws_col;
	*y = term.ws_row;
}

int compute_screen_data()
{
	int x,y;

	get_term_size(&x,&y);
	line = malloc((x+1)*sizeof(char));
	if(!line)
	{
		FATAL("malloc - %s\n",strerror(errno));
		return -1;
	}
	// build line
	// "         +--------------+        "
	// <-startx->|<-menu_width->|
	menu_width = (x * MENU_WIDTH_PERC) / 100;
	startx = ((x - menu_width)/2);
	memset(line,' ',startx);
	line[startx] = '+';
	memset((line+startx+1),'-',menu_width);
	line[startx+menu_width] = '+';
	memset((line+startx+menu_width+1),' ',startx);
	line[x] = '\0';
	return 0;
}
/*
void print_menu2(menu_entry *list)
{
	menu_entry *current;

	// clear screen
	for(;printed_lines;printed_lines--)
		printf("\033[A\033[2K"); // go UP and CLEAR line ( see VT100 reference )
	rewind(stdout);
	ftruncate(1,0);
}
*/
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
