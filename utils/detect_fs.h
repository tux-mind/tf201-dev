#define FS_EXT_OFFSET			56
#define FS_EXT_MAGIC			"\xEF\x53"
// Ext3/4 external journal: INCOMPAT feature JOURNAL_DEV
#define EXT3_TEST1_OFF		96
#define EXT3_TEST1_VAL		0x0008
// Ext3/4 COMPAT feature: HAS_JOURNAL
#define EXT3_TEST2_OFF		92
#define EXT3_TEST2_VAL		0x0004
// Ext4 INCOMPAT features: EXTENTS, 64BIT, FLEX_BG
#define EXT4_TEST1_OFF		96
#define EXT4_TEST1_VAL		0x02C0
// Ext4 RO_COMPAT features: HUGE_FILE, GDT_CSUM, DIR_NLINK, EXTRA_ISIZE
#define EXT4_TEST2_OFF		100
#define EXT4_TEST2_VAL		0x0078
// Ext4 sets min_extra_isize even on external journals
#define EXT4_TEST3_OFF		348
#define EXT4_TEST3_VAL		0x1c
// FAT sectorsize offset, it must be 512,1024,2048 or 4096
#define FAT_SECTORSIZE_OFF		11
// FAT clustersize offset, it must be a power of 2
#define FAT_CLUSTERSIZE_OFF		13
// NTFS signature offset
#define NTFS_TEST_OFF			3
// NTFS signature
#define NTFS_TEST_VAL			"NTFS    "
// how many bytes we should read?
#define BUFFER_SIZE				1024