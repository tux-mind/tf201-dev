#include "mountpoints.h"
#include <stdio.h>
#include <stdlib.h>


void free_mounpoint(mountpoint *item)
{
  if(item->android_mountpoint)
    free(item->android_mountpoint);
  if(item->android_blkdev)
    free(item->android_blkdev);
  if(item->fake_file)
    free(item->fake_file);
  if(item->fake_blkdev)
    free(item->fake_blkdev);
  free(item);
}

void free_list(mountpoint *list)
{
	mountpoint *current;
	for(current=list;current;current=current->next)
		free_mounpoint(current);
}

/* add a mountpoint in the list */
mountpoint *add_mountpoint(mountpoint *list, char *_android_mountpoint, char *_android_blkdev,char *_fake_file, char *_fake_blkdev)
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
	item->android_mountpoint = _android_mountpoint;
	item->android_blkdev = _android_blkdev;
	item->fake_file = _fake_file;
	item->fake_blkdev = _fake_blkdev;
	item->fake_blkdev_fd = item->blkdev_found = 0;
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
	free_mounpoint(current);
	return list;
}