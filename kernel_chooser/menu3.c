#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>

#include "common2.h"
#include "menu3.h"

const char *def_entries[] = 
{
	"boot default config",
	"reboot",
	"shutdown",
	"reboot recovery"
	#ifdef SHELL
	,"emergency shell"
	#endif
};

int def_offsets[] =
{
	0,0,0,0
#ifdef SHELL
	,0
#endif
};

int printed_lines,have_default,startx,menu_width;
char *line,**entries;

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
	int i,def_entries_num;
	// clear screen
	for(;printed_lines;printed_lines--)
		printf("\033[A\033[2K"); // go UP and CLEAR line ( see VT100 reference )
	rewind(stdout);
	ftruncate(1,0);
	printed_lines = def_entries_num = sizeof(def_entries) / sizeof(def_entries[0]);
	printf("%s\n",line);
	// print default entries
	for(i=0;i<def_entries_num;i++)
	{
		//TODO
		printf("%1$*2$s|%1$*4$s%3$s%1$*4$s|\n"," ",startx,def_entries[i],def_offsets[i]);
	}
	// print entries
	printf("%s\n",line);
	for(current=list;current;current=current->next,printed_lines++)
	{
		if((int)current->even)
			printf("%1$*2$s|%1$*4$s%3$s%1$*5$s|\n"," ",startx,current->name,current->xoffset,current->xoffset - 5);
		else
			printf("%1$*2$s|%1$*4$s%3$s%1$*4$s|\n"," ",startx,current->name,current->xoffset);
	}
}

void get_term_size(int *x, int*y)
{
	struct winsize term;

	ioctl(STDOUT_FILENO, TIOCGWINSZ,&term);
	*x = term.ws_col;
	*y = term.ws_row;
}

int compute_screen_data(menu_entry *list)
{
	int x,y,i,def_entries_num;
	menu_entry *current;
	
	def_entries_num = sizeof(def_entries) / sizeof(def_entries[0]);

	get_term_size(&x,&y);
	menu_width = (x * MENU_WIDTH_PERC) / 100;
	startx = ((x - menu_width)/2);
	line = malloc((startx+menu_width+2)*sizeof(char));
	if(!line)
	{
		FATAL("malloc - %s\n",strerror(errno));
		return -1;
	}
	// build line
	// <-----------screen width---------->
	// "         +--------------+\n       "
	// <-startx->|<-menu_width->|
	memset(line,' ',startx);
	line[startx] = '+';
	memset((line+startx+1),'-',menu_width);
	line[startx+menu_width] = '+';
	line[startx+menu_width+1] = '\n';
	//compute default entries offsets
	for(i=0;i<def_entries_num;i++)
		def_offsets[i] = ((menu_width - strlen(def_entries[i]))/2);
	//compute user entries offsets
	// use i as temporary variable
	for(current=list;current;current=current->next)
	{
		i = strlen(current->name);
		current->xoffset = ((menu_width - i)/2);
		if(i%2) // odd
			current->even = (unsigned char)0;
		else
			current->even = (unsigned char)1;
	}
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
