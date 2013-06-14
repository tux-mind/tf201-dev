typedef enum {
	NONE = 0,
	BLKDEV = 1,
	IMAGE_FILE = 2,
	DIRECTORY = 3,
	//CPIO_ARCHIVE = 4
} source_type;

typedef enum {
	WAIT = 1,
	BIND = 2
} _options;

extern const char *options_str[];

/* this is a mountpoint.
 * @android_mountpoint: the mountpoint in the android root
 * @android_blkdev: the blockdev associated with the android_mountpoint, if any.
 * @fake_file: the ext4 file image that will be mounted on the mountpoint. ( just for debugging )
 * @fake_blkdev: the loop device that has the fake fs assigned.
 * @fake_blkdev_fd: the file descriptor of the loop device ( search for fd_to_close in loop_mount3.c )
 * @processed: 1 if we have processed this entry, 0 if not
 * FIXME: rewrite these comments
 */
typedef struct _mountpoint {
	char *mountpoint,
		*android_blkdev,
		*blkdev;
	const char *filesystem;
	_options options;
	int processed,blkdev_fd;
	source_type s_type;
	struct _mountpoint *next;
} mountpoint;

void free_mountpoint(mountpoint *);
void free_list(mountpoint *);
mountpoint *add_mountpoint(mountpoint *, char *, char *);
mountpoint *del_mountpoint(mountpoint *, mountpoint *);