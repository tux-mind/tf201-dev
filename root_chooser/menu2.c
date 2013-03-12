#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>

#include "menu2.h"

int printed_lines;

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

menu_entry *add_entry(menu_entry *list, char *_name, char *_blkdev,char *_kernel, char *_cmdline, char *_initrd)
{
	menu_entry *item;
#ifdef SHELL
	static unsigned short id = 5;
#else
	static unsigned short id = 4;
#endif

	char *name;
	int len;

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

	len = strlen(_name);
	name = malloc((len+1)*sizeof(char));
	if(!name)
	{
		free(item);
		return NULL;
	}
	strncpy(name,_name,len);
	item->id = id++;
	item->name = name;
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
	int i;
	menu_entry *current;
	// clear screen
	for(i=0;i<printed_lines;i++)
		printf("\033[A\033[2K"); // go UP and CLEAR line ( see VT100 reference )
	rewind(stdout);
	ftruncate(1,0);
	// print entries
	// TODO: use chars. ex: 'r' for reboot
	printf("0) boot android\n");
	printf("1) reboot\n");
	printf("2) poweroff\n");
	printf("3) reboot recovery\n");
#ifdef SHELL
	printf("4) emergency shell\n");
#endif
	printf("   ------------------\n");
	for(printed_lines=1,current=list;current;current=current->next,printed_lines++)
		printf("%u) %s\n",current->id,current->name);
}

menu_entry *get_item_by_id(menu_entry *list, int id)
{
	for(;list;list=list->next)
		if(list->id == id)
			return list;
	return NULL;
}
