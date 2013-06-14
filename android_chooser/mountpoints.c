#include "mountpoints.h"
#include <stdio.h>
#include <stdlib.h>

const char *options_str[] = {
	"",
	"wait",
	"bind"
};

void free_mountpoint(mountpoint *item)
{
  if(item->mountpoint)
    free(item->mountpoint);
  if(item->android_blkdev)
    free(item->android_blkdev);
  if(item->blkdev)
    free(item->blkdev);
  free(item);
}

void free_list(mountpoint *list)
{
	mountpoint *current;
	for(current=list;current;current=current->next)
		free_mountpoint(current);
}

/* add a mountpoint in the list */
mountpoint *add_mountpoint(mountpoint *list, char *_blkdev, char *_mountpoint)
{
	mountpoint *item;

	if(!list)
	{
		list = item = malloc(sizeof(mountpoint));
		if(!list)
			return NULL;
	}
	else
	{
		for(item=list;item->next;item=item->next);
		item = item->next = malloc(sizeof(mountpoint));
		if(!item)
			return NULL;
	}
	item->mountpoint = _mountpoint;
	item->blkdev = _blkdev;
	item->blkdev_fd = item->processed = item->s_type = item->options = 0;
	item->filesystem = NULL;
	item->android_blkdev = NULL;
	item->next = NULL;
	return list;
}

mountpoint *del_mountpoint(mountpoint *list, mountpoint *item)
{
	mountpoint *prev,*current;

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
	free_mountpoint(current);
	return list;
}