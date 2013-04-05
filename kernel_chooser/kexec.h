#ifndef KEXEC_H
#define KEXEC_H

#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define USE_BSD
#include <byteswap.h>
#include <endian.h>
#define _GNU_SOURCE

#include "elf.h"
#include "sha256.h"
//#include "unused.h"

#ifndef BYTE_ORDER
#error BYTE_ORDER not defined
#endif

#ifndef LITTLE_ENDIAN
#error LITTLE_ENDIAN not defined
#endif

#ifndef BIG_ENDIAN
#error BIG_ENDIAN not defined
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
#define cpu_to_le16(val) (val)
#define cpu_to_le32(val) (val)
#define cpu_to_le64(val) (val)
#define cpu_to_be16(val) bswap_16(val)
#define cpu_to_be32(val) bswap_32(val)
#define cpu_to_be64(val) bswap_64(val)
#define le16_to_cpu(val) (val)
#define le32_to_cpu(val) (val)
#define le64_to_cpu(val) (val)
#define be16_to_cpu(val) bswap_16(val)
#define be32_to_cpu(val) bswap_32(val)
#define be64_to_cpu(val) bswap_64(val)
#elif BYTE_ORDER == BIG_ENDIAN
#define cpu_to_le16(val) bswap_16(val)
#define cpu_to_le32(val) bswap_32(val)
#define cpu_to_le64(val) bswap_64(val)
#define cpu_to_be16(val) (val)
#define cpu_to_be32(val) (val)
#define cpu_to_be64(val) (val)
#define le16_to_cpu(val) bswap_16(val)
#define le32_to_cpu(val) bswap_32(val)
#define le64_to_cpu(val) bswap_64(val)
#define be16_to_cpu(val) (val)
#define be32_to_cpu(val) (val)
#define be64_to_cpu(val) (val)
#else
#error unknwon BYTE_ORDER
#endif


/*
 * This function doesn't actually exist.  The idea is that when someone
 * uses the macros below with an unsupported size (datatype), the linker
 * will alert us to the problem via an unresolved reference error.
 */
//extern unsigned long bad_unaligned_access_length (void);

#define get_unaligned(loc) \
({ \
	__typeof__(*(loc)) _v; \
	size_t size = sizeof(*(loc)); \
	switch(size) {  \
	case 1: case 2: case 4: case 8: \
		memcpy(&_v, (loc), size); \
		break; \
	default: \
		_v = bad_unaligned_access_length(); \
		break; \
	} \
	_v; \
})

#define put_unaligned(value, loc) \
do { \
	size_t size = sizeof(*(loc)); \
	__typeof__(*(loc)) _v = value; \
	switch(size) { \
	case 1: case 2: case 4: case 8: \
		memcpy((loc), &_v, size); \
		break; \
	default: \
		bad_unaligned_access_length(); \
		break; \
	} \
} while(0)

struct mem_ehdr {
	unsigned ei_class;
	unsigned ei_data;
	unsigned e_type;
	unsigned e_machine;
	unsigned e_version;
	unsigned e_flags;
	unsigned e_phnum;
	unsigned e_shnum;
	unsigned e_shstrndx;
	unsigned long e_entry;
	unsigned long e_phoff;
	unsigned long e_shoff;
	unsigned e_notenum;
	struct mem_phdr *e_phdr;
	struct mem_shdr *e_shdr;
	struct mem_note *e_note;
	unsigned long rel_addr, rel_size;
};

struct mem_phdr {
	unsigned long p_paddr;
	unsigned long p_vaddr;
	unsigned long p_filesz;
	unsigned long p_memsz;
	unsigned long p_offset;
	const char *p_data;
	unsigned p_type;
	unsigned p_flags;
	unsigned p_align;
};

struct mem_shdr {
	unsigned sh_name;
	unsigned sh_type;
	unsigned long sh_flags;
	unsigned long sh_addr;
	unsigned long sh_offset;
	unsigned long sh_size;
	unsigned sh_link;
	unsigned sh_info;
	unsigned long sh_addralign;
	unsigned long sh_entsize;
	const unsigned char *sh_data;
};

struct mem_sym {
	unsigned long st_name;   /* Symbol name (string tbl index) */
	unsigned char st_info;   /* No defined meaning, 0 */
	unsigned char st_other;  /* Symbol type and binding */
	unsigned long st_shndx;  /* Section index */
	unsigned long st_value;  /* Symbol value */
	unsigned long st_size;   /* Symbol size */
};

struct  mem_rela {
	unsigned long r_offset;
	unsigned long r_sym;
	unsigned long r_type;
	unsigned long r_addend;
};

struct mem_note {
	unsigned n_type;
	unsigned n_descsz;
	const char *n_name;
	const void *n_desc;
};

/* The definition of an ELF note does not vary depending
 * on ELFCLASS.
 */
typedef struct
{
	uint32_t n_namesz;		/* Length of the note's name.  */
	uint32_t n_descsz;		/* Length of the note's descriptor.  */
	uint32_t n_type;		/* Type of the note.  */
} ElfNN_Nhdr;

/* Misc flags */

#define ELF_SKIP_FILESZ_CHECK		0x00000001

struct kexec_segment {
	const void *buf;
	size_t bufsz;
	const void *mem;
	size_t memsz;
};

struct memory_range {
	unsigned long long start, end;
	unsigned type;
#define RANGE_RAM	0
#define RANGE_RESERVED	1
#define RANGE_ACPI	2
#define RANGE_ACPI_NVS	3
#define RANGE_UNCACHED	4
};

#define MAX_MEMORY_RANGES 64
static struct memory_range memory_range[MAX_MEMORY_RANGES];

#define BOOT_PARAMS_SIZE 1536

struct tag_header {
	uint32_t size;
	uint32_t tag;
};

/* The list must start with an ATAG_CORE node */
#define ATAG_CORE       0x54410001

struct tag_core {
	uint32_t flags;	    /* bit 0 = read-only */
	uint32_t pagesize;
	uint32_t rootdev;
};

/* it is allowed to have multiple ATAG_MEM nodes */
#define ATAG_MEM	0x54410002

struct tag_mem32 {
	uint32_t   size;
	uint32_t   start;  /* physical start address */
};

/* describes where the compressed ramdisk image lives (virtual address) */
/*
 * this one accidentally used virtual addresses - as such,
 * it's deprecated.
 */
#define ATAG_INITRD     0x54410005

/* describes where the compressed ramdisk image lives (physical address) */
#define ATAG_INITRD2    0x54420005

struct tag_initrd {
        uint32_t start;    /* physical start address */
        uint32_t size;     /* size of compressed ramdisk image in bytes */
};

/* command line: \0 terminated string */
#define ATAG_CMDLINE    0x54410009

struct tag_cmdline {
	char    cmdline[1];     /* this is the minimum size */
};

/* The list ends with an ATAG_NONE node. */
#define ATAG_NONE       0x00000000

struct tag {
	struct tag_header hdr;
	union {
		struct tag_core	 core;
		struct tag_mem32	mem;
		struct tag_initrd       initrd;
		struct tag_cmdline      cmdline;
	} u;
};

#define tag_next(t)     ((struct tag *)((uint32_t *)(t) + (t)->hdr.size))
#define byte_size(t)    ((t)->hdr.size << 2)
#define tag_size(type)  ((sizeof(struct tag_header) + sizeof(struct type) + 3) >> 2)
#define KEXEC_MAX_SEGMENTS 16

struct sha256_region {
	uint64_t start;
	uint64_t len;
};

#define SHA256_REGIONS 16

#define	LINUX_REBOOT_MAGIC1	0xfee1dead
#define	LINUX_REBOOT_MAGIC2	672274793
#define	LINUX_REBOOT_MAGIC2A	85072278
#define	LINUX_REBOOT_MAGIC2B	369367448

#define	LINUX_REBOOT_CMD_RESTART	0x01234567
#define	LINUX_REBOOT_CMD_HALT		0xCDEF0123
#define	LINUX_REBOOT_CMD_CAD_ON		0x89ABCDEF
#define	LINUX_REBOOT_CMD_CAD_OFF	0x00000000
#define	LINUX_REBOOT_CMD_POWER_OFF	0x4321FEDC
#define	LINUX_REBOOT_CMD_RESTART2	0xA1B2C3D4
#define LINUX_REBOOT_CMD_EXEC_KERNEL    0x18273645
#define LINUX_REBOOT_CMD_KEXEC_OLD	0x81726354
#define LINUX_REBOOT_CMD_KEXEC_OLD2	0x18263645
#define LINUX_REBOOT_CMD_KEXEC		0x45584543

#define KEXEC_HARDBOOT 0x00000004
#define KEXEC_ARCH_ARM     (40 << 16)
#define KEXEC_FLAGS (KEXEC_ARCH_ARM | KEXEC_HARDBOOT )

/*
 * Operating System Codes
 */
#define IH_OS_INVALID		0	/* Invalid OS	*/
#define IH_OS_OPENBSD		1	/* OpenBSD	*/
#define IH_OS_NETBSD		2	/* NetBSD	*/
#define IH_OS_FREEBSD		3	/* FreeBSD	*/
#define IH_OS_4_4BSD		4	/* 4.4BSD	*/
#define IH_OS_LINUX		5	/* Linux	*/
#define IH_OS_SVR4		6	/* SVR4		*/
#define IH_OS_ESIX		7	/* Esix		*/
#define IH_OS_SOLARIS		8	/* Solaris	*/
#define IH_OS_IRIX		9	/* Irix		*/
#define IH_OS_SCO		10	/* SCO		*/
#define IH_OS_DELL		11	/* Dell		*/
#define IH_OS_NCR		12	/* NCR		*/
#define IH_OS_LYNXOS		13	/* LynxOS	*/
#define IH_OS_VXWORKS		14	/* VxWorks	*/
#define IH_OS_PSOS		15	/* pSOS		*/
#define IH_OS_QNX		16	/* QNX		*/
#define IH_OS_U_BOOT		17	/* Firmware	*/
#define IH_OS_RTEMS		18	/* RTEMS	*/
#define IH_OS_ARTOS		19	/* ARTOS	*/
#define IH_OS_UNITY		20	/* Unity OS	*/
#define IH_OS_INTEGRITY		21	/* INTEGRITY	*/

/*
 * CPU Architecture Codes (supported by Linux)
 */
#define IH_ARCH_INVALID		0	/* Invalid CPU	*/
#define IH_ARCH_ALPHA		1	/* Alpha	*/
#define IH_ARCH_ARM		2	/* ARM		*/
#define IH_ARCH_I386		3	/* Intel x86	*/
#define IH_ARCH_IA64		4	/* IA64		*/
#define IH_ARCH_MIPS		5	/* MIPS		*/
#define IH_ARCH_MIPS64		6	/* MIPS	 64 Bit */
#define IH_ARCH_PPC		7	/* PowerPC	*/
#define IH_ARCH_S390		8	/* IBM S390	*/
#define IH_ARCH_SH		9	/* SuperH	*/
#define IH_ARCH_SPARC		10	/* Sparc	*/
#define IH_ARCH_SPARC64		11	/* Sparc 64 Bit */
#define IH_ARCH_M68K		12	/* M68K		*/
#define IH_ARCH_NIOS		13	/* Nios-32	*/
#define IH_ARCH_MICROBLAZE	14	/* MicroBlaze   */
#define IH_ARCH_NIOS2		15	/* Nios-II	*/
#define IH_ARCH_BLACKFIN	16	/* Blackfin	*/
#define IH_ARCH_AVR32		17	/* AVR32	*/
#define IH_ARCH_ST200	        18	/* STMicroelectronics ST200  */

#define IH_TYPE_INVALID		0	/* Invalid Image		*/
#define IH_TYPE_STANDALONE	1	/* Standalone Program		*/
#define IH_TYPE_KERNEL		2	/* OS Kernel Image		*/
#define IH_TYPE_RAMDISK		3	/* RAMDisk Image		*/
#define IH_TYPE_MULTI		4	/* Multi-File Image		*/
#define IH_TYPE_FIRMWARE	5	/* Firmware Image		*/
#define IH_TYPE_SCRIPT		6	/* Script file			*/
#define IH_TYPE_FILESYSTEM	7	/* Filesystem Image (any type)	*/
#define IH_TYPE_FLATDT		8	/* Binary Flat Device Tree Blob	*/
#define IH_TYPE_KWBIMAGE	9	/* Kirkwood Boot Image		*/

/*
 * Compression Types
 */
#define IH_COMP_NONE		0	/*  No	 Compression Used	*/
#define IH_COMP_GZIP		1	/* gzip	 Compression Used	*/
#define IH_COMP_BZIP2		2	/* bzip2 Compression Used	*/
#define IH_COMP_LZMA		3	/* lzma  Compression Used	*/
#define IH_COMP_LZO		4	/* lzo   Compression Used	*/

#define IH_MAGIC	0x27051956	/* Image Magic Number		*/
#define IH_NMLEN		32	/* Image Name Length		*/

/*
 * all data in network byte order (aka natural aka bigendian)
 */

typedef struct image_header {
	uint32_t	ih_magic;	/* Image Header Magic Number	*/
	uint32_t	ih_hcrc;	/* Image Header CRC Checksum	*/
	uint32_t	ih_time;	/* Image Creation Timestamp	*/
	uint32_t	ih_size;	/* Image Data Size		*/
	uint32_t	ih_load;	/* Data	 Load  Address		*/
	uint32_t	ih_ep;		/* Entry Point Address		*/
	uint32_t	ih_dcrc;	/* Image Data CRC Checksum	*/
	uint8_t		ih_os;		/* Operating System		*/
	uint8_t		ih_arch;	/* CPU architecture		*/
	uint8_t		ih_type;	/* Image Type			*/
	uint8_t		ih_comp;	/* Compression Type		*/
	uint8_t		ih_name[IH_NMLEN];	/* Image Name		*/
} image_header_t;


struct kexec_info {
	struct kexec_segment *segment;
	int nr_segments;
	struct memory_range *memory_range;
	int memory_ranges;
	void *entry;
	struct mem_ehdr rhdr;
	unsigned long backup_start;
	unsigned long kexec_flags;
	unsigned long kern_vaddr_start;
	unsigned long kern_paddr_start;
	unsigned long kern_size;
};

struct arch_map_entry {
	const char *machine;
	unsigned long arch;
};

struct Image_info {
	const char *buf;
	off_t len;
	unsigned int base;
	unsigned int ep;
};

extern const struct arch_map_entry arches[];
int get_memory_ranges(struct memory_range **range, int *ranges);
int valid_memory_range(struct kexec_info *info,
		       unsigned long sstart, unsigned long send);
int sort_segments(struct kexec_info *info);
unsigned long locate_hole(struct kexec_info *info,
	unsigned long hole_size, unsigned long hole_align,
	unsigned long hole_min, unsigned long hole_max,
	int hole_end);

typedef int (probe_t)(const char *kernel_buf, off_t kernel_size);
typedef int (load_t )(int argc, char **argv,
	const char *kernel_buf, off_t kernel_size,
	struct kexec_info *info);
struct file_type {
	const char *name;
	probe_t *probe;
	load_t  *load;
};

extern struct file_type file_type[];
extern int file_types;

//from other files
char *zlib_decompress_file(const char *, off_t *);
char *lzma_decompress_file(const char *, off_t *);

/*
#define OPT_HELP		'h'
#define OPT_VERSION		'v'
#define OPT_DEBUG		'd'
#define OPT_FORCE		'f'
#define OPT_NOIFDOWN		'x'
#define OPT_EXEC		'e'
#define OPT_LOAD		'l'
#define OPT_UNLOAD		'u'
#define OPT_TYPE		't'
#define OPT_PANIC		'p'
#define OPT_MEM_MIN             256
#define OPT_MEM_MAX             257
#define OPT_REUSE_INITRD	258
#define OPT_LOAD_PRESERVE_CONTEXT 259
#define OPT_LOAD_JUMP_BACK_HELPER 260
#define OPT_ENTRY		261
#define OPT_LOAD_HARDBOOT	262
#define OPT_MAX			263
#define KEXEC_OPTIONS \
	{ "help",		0, 0, OPT_HELP }, \
	{ "version",		0, 0, OPT_VERSION }, \
	{ "force",		0, 0, OPT_FORCE }, \
	{ "no-ifdown",		0, 0, OPT_NOIFDOWN }, \
	{ "load",		0, 0, OPT_LOAD }, \
	{ "unload",		0, 0, OPT_UNLOAD }, \
	{ "exec",		0, 0, OPT_EXEC }, \
	{ "load-preserve-context", 0, 0, OPT_LOAD_PRESERVE_CONTEXT}, \
	{ "load-jump-back-helper", 0, 0, OPT_LOAD_JUMP_BACK_HELPER }, \
	{ "entry",		1, 0, OPT_ENTRY }, \
	{ "type",		1, 0, OPT_TYPE }, \
	{ "load-panic",         0, 0, OPT_PANIC }, \
	{ "mem-min",		1, 0, OPT_MEM_MIN }, \
	{ "mem-max",		1, 0, OPT_MEM_MAX }, \
	{ "reuseinitrd",	0, 0, OPT_REUSE_INITRD }, \
	{ "load-hardboot",	0, 0, OPT_LOAD_HARDBOOT}, \

#define KEXEC_OPT_STR "hvdfxluet:p"

extern char *slurp_file(const char *filename, off_t *r_size);
extern char *slurp_file_len(const char *filename, off_t size);
extern char *slurp_decompress_file(const char *filename, off_t *r_size);
extern unsigned long virt_to_phys(unsigned long addr);
extern void add_segment(struct kexec_info *info,
	const void *buf, size_t bufsz, unsigned long base, size_t memsz);
extern void add_segment_phys_virt(struct kexec_info *info,
	const void *buf, size_t bufsz, unsigned long base, size_t memsz);
extern unsigned long add_buffer(struct kexec_info *info,
	const void *buf, unsigned long bufsz, unsigned long memsz,
	unsigned long buf_align, unsigned long buf_min, unsigned long buf_max,
	int buf_end);
extern unsigned long add_buffer_virt(struct kexec_info *info,
	const void *buf, unsigned long bufsz, unsigned long memsz,
	unsigned long buf_align, unsigned long buf_min, unsigned long buf_max,
	int buf_end);
extern unsigned long add_buffer_phys_virt(struct kexec_info *info,
	const void *buf, unsigned long bufsz, unsigned long memsz,
	unsigned long buf_align, unsigned long buf_min, unsigned long buf_max,
	int buf_end, int phys);
extern void arch_reuse_initrd(void);

extern int ifdown(void);

extern char purgatory[];
extern size_t purgatory_size;

#define BOOTLOADER "kexec"
#define BOOTLOADER_VERSION PACKAGE_VERSION

void arch_usage(void);
int arch_process_options(int argc, char **argv);
int arch_compat_trampoline(struct kexec_info *info);
void arch_update_purgatory(struct kexec_info *info);
int is_crashkernel_mem_reserved(void);
char *get_command_line(void);

int kexec_iomem_for_each_line(char *match,
			      int (*callback)(void *data,
					      int nr,
					      char *str,
					      unsigned long base,
					      unsigned long length),
			      void *data);
int parse_iomem_single(char *str, uint64_t *start, uint64_t *end);
const char * proc_iomem(void);

int arch_init(void);

extern int add_backup_segments(struct kexec_info *info,
			       unsigned long backup_base,
			       unsigned long backup_size);

#define MAX_LINE	160

#ifdef DEBUG
#define dbgprintf(_args...) do {printf(_args);} while(0)
#else
static inline int __attribute__ ((format (printf, 1, 2)))
	dbgprintf(const char *UNUSED(fmt), ...) {return 0;}
#endif

char *concat_cmdline(const char *base, const char *append);
*/
#endif /* KEXEC_H */
