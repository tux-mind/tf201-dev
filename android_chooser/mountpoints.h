/* this is a mountpoint.
 * @android_mountpoint: the mountpoint in the android root
 * @android_blkdev: the blockdev associated with the android_mountpoint, if any.
 * @fake_file: the ext4 file image that will be mounted on the mountpoint. ( just for debugging )
 * @fake_blkdev: the loop device that has the fake fs assigned.
 * @blkdev_found: 1 if udev has found the android_blkdev, 0 otherwise.
 */
typedef struct _mountpoint {
  char 	*android_mountpoint,
	*android_blkdev,
	*fake_file,
	*fake_blkdev;
  int blkdev_found;
  struct _mountpoint *next;
} mountpoint;

void free_entry(mountpoint *);
void free_list(mountpoint *);
mountpoint *add_mountpoint(mountpoint *, char *, char *,char *, char *);
mountpoint *del_mountpoint(mountpoint *, mountpoint *);