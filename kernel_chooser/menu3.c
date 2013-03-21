#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <curses.h>
#include <menu.h>

#include "menu3.h"
#include "common2.h"

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

menu_entry *get_item_by_id(menu_entry *list, int id)
{
	for(;list;list=list->next)
		if(list->id == id)
			return list;
	return NULL;
}
