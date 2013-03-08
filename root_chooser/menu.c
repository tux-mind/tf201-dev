#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>

#include "menu.h"

int printed_lines;

void free_entry(menu_entry *item)
{
	char **argv;

	if(item->name)
		free(item->name);
	if(item->blkdev)
		free(item->blkdev);
	if(item->root)
		free(item->root);
	if(item->init_argv)
	{
		for(argv=item->init_argv;*argv;argv++)
			free(*argv);
		free(item->init_argv);
	}
	free(item);
}

void free_menu(menu_entry *list)
{
	menu_entry *current;
	for(current=list;current;current=current->next)
		free_entry(current);
}

menu_entry *add_entry(menu_entry *list, char *_name, char *_blkdev, char *_root, char **_init_argv)
{
	menu_entry *item;
	static unsigned short id = 1;
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
	item->root = _root;
	item->init_argv = _init_argv;
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
	printf("0) Boot android\n");
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