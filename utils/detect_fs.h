#define FS_EXT_OFFSET			0x438
#define FS_EXT(x)				(get_le_short(x+FS_EXT_OFFSET) == 0xEF53)
#define EXT_JOURNAL_OFF			0x45c
#define EXT_INCOMPAT_OFF		0x460
#define EXT_RO_COMPAT_OFF		0x464
#define EXT_JOURNAL(x)			(get_le_long(x+EXT_JOURNAL_OFF) & 0x4)
#define EXT_SMALL_INCOMPAT(x)	(get_le_long(x+EXT_INCOMPAT_OFF) < 0x40)
#define EXT_SMALL_RO_COMPAT(x)	(get_le_long(x+EXT_RO_COMPAT_OFF) < 0x8)
// how many bytes we should read?
#define BUFFER_SIZE				1200