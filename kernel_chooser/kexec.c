/*
 * kexec: Linux boots Linux
 *
 * Copyright (C) 2003-2005  Eric Biederman (ebiederm@xmission.com)
 *
 * Modified (2007-05-15) by Francesco Chiechi to rudely handle mips platform
 * Modified (2013-03-04) by Dragano Massimo for kernel_chooser
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation (version 2 of the License).
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <zlib.h>
#include <syscall.h>
#include <sys/syscall.h>
#ifndef _O_BINARY
#define _O_BINARY 0
#endif

#include "kexec.h"
#include "common2.h"

unsigned long long mem_min, mem_max;

char *slurp_file(const char *filename, off_t *r_size)
{
	int fd;
	char *buf;
	off_t size, progress;
	ssize_t result;
	struct stat stats;


	if (!filename) {
		*r_size = 0;
		return NULL;
	}
	fd = open(filename, O_RDONLY | _O_BINARY);
	if (fd < 0) {
		ERROR("cannot open \"%s\" - %s\n",filename, strerror(errno));
		return NULL;
	}
	result = fstat(fd, &stats);
	if (result < 0) {
		ERROR("cannot stat \"%s\" - %s\n",filename, strerror(errno));
		close(fd);
		return NULL;
	}
	if (S_ISCHR(stats.st_mode)) {
		size = lseek(fd, 0, SEEK_END);
		if (size < 0)
		{
			ERROR("can not seek file \"%s\" - %s\n", filename,strerror(errno));
			close(fd);
			return NULL;
		}
		// use progress as temporary variable
		progress = lseek(fd, 0, SEEK_SET);
		if (progress < 0)
		{
			ERROR("can not seek to the begin of file \"%s\" - %s\n",filename, strerror(errno));
			close(fd);
			return NULL;
		}
	} else {
		size = stats.st_size;
	}

	*r_size = size;
	buf = malloc(size);
	if(!buf)
	{
		FATAL("malloc - %s\n",strerror(errno));
		close(fd);
		return NULL;
	}

	progress = 0;
	while(progress < size) {
		result = read(fd, buf + progress, size - progress);
		if (result < 0) {
			if ((errno == EINTR) ||	(errno == EAGAIN))
				continue;
			ERROR("read on \"%s\" of %ld bytes failed - %s\n", filename,(size - progress)+ 0UL, strerror(errno));
			free(buf);
			close(fd);
			return NULL;
		}
		if (result == 0)
		{
			ERROR("read on \"%s\" ended before stat said it should\n", filename);
			free(buf);
			close(fd);
			return NULL;
		}
		progress += result;
	}
	result = close(fd);
	if (result < 0) {
		ERROR("close of \"%s\" failed - %s\n", filename, strerror(errno));
		free(buf);
		return NULL;
	}
	return buf;
}

char *slurp_decompress_file(const char *filename, off_t *r_size)
{
	char *kernel_buf;

	kernel_buf = zlib_decompress_file(filename, r_size);
	if (!kernel_buf) {
		kernel_buf = lzma_decompress_file(filename, r_size);
		if (!kernel_buf)
			return slurp_file(filename, r_size);
	}
	return kernel_buf;
}

int get_memory_ranges(struct memory_range **range, int *ranges)
{
	const char *iomem = "/proc/iomem";
	int memory_ranges = 0;
	char line[MAX_LINE];
	FILE *fp;
	fp = fopen(iomem, "r");
	if (!fp) {
		ERROR("cannot open \"%s\" - %s\n",iomem, strerror(errno));
		return -1;
	}

	while(fgets(line, sizeof(line), fp) != NULL) {
		unsigned long long start, end;
		char *str;
		int type;
		int consumed;
		int count;
		if (memory_ranges >= MAX_MEMORY_RANGES)
			break;
		count = sscanf(line, "%Lx-%Lx : %n",
			&start, &end, &consumed);
		if (count != 2)
			continue;
		str = line + consumed;
		end = end + 1;

		if (memcmp(str, "System RAM\n", 11) == 0) {
			type = RANGE_RAM;
		}
		else if (memcmp(str, "reserved\n", 9) == 0) {
			type = RANGE_RESERVED;
		}
		else {
			continue;
		}

		memory_range[memory_ranges].start = start;
		memory_range[memory_ranges].end = end;
		memory_range[memory_ranges].type = type;
		memory_ranges++;
	}
	fclose(fp);
	*range = memory_range;
	*ranges = memory_ranges;
	return 0;
}

unsigned long locate_hole(struct kexec_info *info,
	unsigned long hole_size, unsigned long hole_align,
	unsigned long hole_min, unsigned long hole_max,
	int hole_end)
{
	int i, j;
	struct memory_range *mem_range;
	int max_mem_ranges, mem_ranges;
	unsigned long hole_base;

	/* Set an intial invalid value for the hole base */
	hole_base = ULONG_MAX;

	if (hole_end == 0) {
		ERROR("invalid hole end argument of 0 specified to locate_hole");
		return hole_base;
	}

	/* Ensure I have a sane alignment value */
	if (hole_align == 0) {
		hole_align = 1;
	}
	/* Align everything to at least a page size boundary */
	if (hole_align < (unsigned long)getpagesize()) {
		hole_align = getpagesize();
	}

	/* Compute the free memory ranges */
	max_mem_ranges = info->memory_ranges + info->nr_segments;
	mem_range = malloc(max_mem_ranges *sizeof(struct memory_range));
	mem_ranges = 0;

	if(!mem_range)
		return hole_base;

	/* Perform a merge on the 2 sorted lists of memory ranges  */
	for (j = 0, i = 0; i < info->memory_ranges; i++) {
		unsigned long long sstart, send;
		unsigned long long mstart, mend;
		mstart = info->memory_range[i].start;
		mend = info->memory_range[i].end;
		if (info->memory_range[i].type != RANGE_RAM)
			continue;
		while ((j < info->nr_segments) &&
		       (((unsigned long)info->segment[j].mem) <= mend)) {
			sstart = (unsigned long)info->segment[j].mem;
			send = sstart + info->segment[j].memsz -1;
			if (mstart < sstart) {
				mem_range[mem_ranges].start = mstart;
				mem_range[mem_ranges].end = sstart -1;
				mem_range[mem_ranges].type = RANGE_RAM;
				mem_ranges++;
			}
			mstart = send +1;
			j++;
		}
		if (mstart < mend) {
			mem_range[mem_ranges].start = mstart;
			mem_range[mem_ranges].end = mend;
			mem_range[mem_ranges].type = RANGE_RAM;
			mem_ranges++;
		}
	}
	/* Now find the end of the last memory_range I can use */
	for (i = 0; i < mem_ranges; i++) {
		unsigned long long start, end, size;
		start = mem_range[i].start;
		end   = mem_range[i].end;
		/* First filter the range start and end values
		 * through the lens of mem_min, mem_max and hole_align.
		 */
		if (start < mem_min) {
			start = mem_min;
		}
		if (start < hole_min) {
			start = hole_min;
		}
		start = (start + hole_align - 1) &
			~((unsigned long long)hole_align - 1);
		if (end > mem_max) {
			end = mem_max;
		}
		if (end > hole_max) {
			end = hole_max;
		}
		/* Is this still a valid memory range? */
		if ((start >= end) || (start >= mem_max) || (end <= mem_min)) {
			continue;
		}
		/* Is there enough space left so we can use it? */
		size = end - start;
		if (size >= hole_size) {
			if (hole_end > 0) {
				hole_base = start;
				break;
			} else {
				hole_base = (end - hole_size) &
					~((unsigned long long)hole_align - 1);
			}
		}
	}
	free(mem_range);
	if (hole_base == ULONG_MAX) {
		ERROR("could not find a free area of memory of %lx bytes...\n", hole_size);
		return ULONG_MAX;
	}
	if ((hole_base + hole_size)  > hole_max) {
		ERROR("could not find a free area of memory below: %lx...\n", hole_max);
		return ULONG_MAX;
	}
	return hole_base;
}

static
struct tag * atag_read_tags(void)
{
	static unsigned long buf[BOOT_PARAMS_SIZE];
	const char fn[]= "/proc/atags";
	FILE *fp;
	fp = fopen(fn, "r");
	if (!fp) {
		ERROR("cannot open \"%s\" - %s\n", fn, strerror(errno));
		return NULL;
	}

	if (!fread(buf, sizeof(buf[1]), BOOT_PARAMS_SIZE, fp)) {
		fclose(fp);
		return NULL;
	}

	if (ferror(fp)) {
		ERROR("cannot read \"%s\" - %s\n",fn, strerror(errno));
		fclose(fp);
		return NULL;
	}

	fclose(fp);
	return (struct tag *) buf;
}

int add_segment_phys_virt(struct kexec_info *info,
	const void *buf, size_t bufsz,
	unsigned long base, size_t memsz)
{
	unsigned long last;
	size_t size;
	int pagesize;

	if (bufsz > memsz) {
		bufsz = memsz;
	}
	/* Forget empty segments */
	if (memsz == 0) {
		return 0;
	}

	/* Round memsz up to a multiple of pagesize */
	pagesize = getpagesize();
	memsz = (memsz + (pagesize - 1)) & ~(pagesize - 1);

	/* Verify base is pagesize aligned.
	 * Finding a way to cope with this problem
	 * is important but for now error so at least
	 * we are not surprised by the code doing the wrong
	 * thing.
	 */
	if (base & (pagesize -1)) {
		ERROR("base address: %lx is not page aligned\n", base);
		return -1;
	}

	last = base + memsz -1;
	if (!valid_memory_range(info, base, last)) {
		ERROR("invalid memory segment %p - %p\n",(void *)base, (void *)last);
		return -1;
	}

	size = (info->nr_segments + 1) * sizeof(info->segment[0]);
	//HACK: use last as temporary variable
	last = (unsigned long)info->segment;
	info->segment = realloc(info->segment, size);
	if(!info->segment)
	{
		free((void*)last);
		ERROR("realloc - %s\n",strerror(errno));
		return -1;
	}
	info->segment[info->nr_segments].buf   = buf;
	info->segment[info->nr_segments].bufsz = bufsz;
	info->segment[info->nr_segments].mem   = (void *)base;
	info->segment[info->nr_segments].memsz = memsz;
	info->nr_segments++;
	if (info->nr_segments > KEXEC_MAX_SEGMENTS) {
		WARN("kernel segment limit reached. This will likely fail\n");
	}
	return 0;
}

static
int atag_arm_load(struct kexec_info *info, unsigned long base,
	const char *command_line, off_t command_line_len,
	const char *initrd, off_t initrd_len, off_t initrd_off)
{
	struct tag *saved_tags = atag_read_tags();
	char *buf;
	off_t len;
	struct tag *params;
	uint32_t *initrd_start = NULL;

	buf = malloc(getpagesize());
	if (!buf) {
		ERROR("compiling ATAGs - %s\n",strerror(errno));
		return -1;
	}

	memset(buf, 0xff, getpagesize());
	params = (struct tag *)buf;

	if (saved_tags) {
		// Copy tags
		saved_tags = (struct tag *) saved_tags; // ?? TODO: comment out this line
		while(byte_size(saved_tags)) {
			switch (saved_tags->hdr.tag) {
			case ATAG_INITRD:
			case ATAG_INITRD2:
			case ATAG_CMDLINE:
			case ATAG_NONE:
				// skip these tags
				break;
			default:
				// copy all other tags
				memcpy(params, saved_tags, byte_size(saved_tags));
				params = tag_next(params);
			}
			saved_tags = tag_next(saved_tags);
		}
	} else {
		params->hdr.size = 2;
		params->hdr.tag = ATAG_CORE;
		params = tag_next(params);
	}

	if (initrd) {
		params->hdr.size = tag_size(tag_initrd);
		params->hdr.tag = ATAG_INITRD2;
		initrd_start = &params->u.initrd.start;
		params->u.initrd.size = initrd_len;
		params = tag_next(params);
	}

	if (command_line) {
		params->hdr.size = (sizeof(struct tag_header) + command_line_len + 3) >> 2;
		params->hdr.tag = ATAG_CMDLINE;
		memcpy(params->u.cmdline.cmdline, command_line,
			command_line_len);
		params->u.cmdline.cmdline[command_line_len - 1] = '\0';
		params = tag_next(params);
	}

	params->hdr.size = 0;
	params->hdr.tag = ATAG_NONE;

	len = ((char *)params - buf) + sizeof(struct tag_header);

	add_segment_phys_virt(info, buf, len, base, len);

	if (initrd) {
		*initrd_start = locate_hole(info, initrd_len, getpagesize(),initrd_off, ULONG_MAX, INT_MAX);
		if (*initrd_start == ULONG_MAX)
		{
			free(buf);
			return -1;
		}
		add_segment_phys_virt(info, initrd, initrd_len, *initrd_start, initrd_len);
	}

	return 0;
}

int zImage_arm_load(const char *buf, char *command_line, char *ramdisk, off_t len, struct kexec_info *info)
{
	unsigned long base;
	unsigned int atag_offset = 0x1000; /* 4k offset from memory start */
	unsigned int offset = 0x8000;      /* 32k offset from memory start */
	off_t command_line_len;
	char *ramdisk_buf;
	off_t ramdisk_length;
	off_t ramdisk_offset;

	command_line_len = 0;
	ramdisk_buf = NULL;
	ramdisk_length = 0;

	if (command_line) {
		command_line_len = strlen(command_line) + 1;
		if (command_line_len > COMMAND_LINE_SIZE)
			command_line_len = COMMAND_LINE_SIZE;
	}
	if (ramdisk) {
		ramdisk_buf = slurp_file(ramdisk, &ramdisk_length);
		if(!ramdisk_buf)
			return -1;
	}

	base = locate_hole(info,len+offset,0,0,ULONG_MAX,INT_MAX);

	if (base == ULONG_MAX)
		return -1;

	/* assume the maximum kernel compression ratio is 4,
	 * and just to be safe, place ramdisk after that
	 */
	ramdisk_offset = base + len * 4;

	if (atag_arm_load(info, base + atag_offset,
			 command_line, command_line_len,
			 ramdisk_buf, ramdisk_length, ramdisk_offset))
		return -1;

	add_segment_phys_virt(info, buf, len, base + offset, len);

	info->entry = (void*)base + offset;

	return 0;
}

int uImage_probe(const char *buf, off_t len, unsigned int arch)
{
	struct image_header header;
	unsigned int crc;
	unsigned int hcrc;

	if ((uintmax_t)len < (uintmax_t)sizeof(header))
		return -1;

	memcpy(&header, buf, sizeof(header));
	if (be32_to_cpu(header.ih_magic) != IH_MAGIC)
		return -1;
	hcrc = be32_to_cpu(header.ih_hcrc);
	header.ih_hcrc = 0;
	crc = crc32(0, (void *)&header, sizeof(header));
	if (crc != hcrc) {
		ERROR("Header checksum of the uImage does not match\n");
		return -1;
	}
	if (header.ih_type != IH_TYPE_KERNEL) {
		ERROR("uImage type %d unsupported\n", header.ih_type);
		return -1;
	}

	if (header.ih_os != IH_OS_LINUX) {
		ERROR("uImage os %d unsupported\n", header.ih_os);
		return -1;
	}

	if (header.ih_arch != arch) {
		ERROR("uImage arch %d unsupported\n", header.ih_arch);
		return -1;
	}

	switch (header.ih_comp) {
	case IH_COMP_NONE:
	case IH_COMP_GZIP:
		break;
	default:
		ERROR("uImage uses unsupported compression method\n");
		return -1;
	}

	if (be32_to_cpu(header.ih_size) > len - sizeof(header)) {
		ERROR("uImage header claims that image has %d bytes\n",be32_to_cpu(header.ih_size));
		ERROR("we read only %ld bytes.\n", len - sizeof(header));
		return -1;
	}
	crc = crc32(0, (void *)buf + sizeof(header), len - sizeof(header));
	if (crc != be32_to_cpu(header.ih_dcrc)) {
		ERROR("The data CRC does not match. Computed: %08x expected %08x\n", crc,be32_to_cpu(header.ih_dcrc));
		return -1;
	}
	return 0;
}

int uImage_load(const char *buf, char *cmdline, char *initrd, off_t len, struct kexec_info *info)
{
	return zImage_arm_load(buf + sizeof(struct image_header), cmdline, initrd,len - sizeof(struct image_header), info);
}

int valid_memory_range(struct kexec_info *info,
		       unsigned long sstart, unsigned long send)
{
	int i;
	if (sstart > send) {
		return 0;
	}
	if ((send > mem_max) || (sstart < mem_min)) {
		return 0;
	}
	for (i = 0; i < info->memory_ranges; i++) {
		unsigned long mstart, mend;
		/* Only consider memory ranges */
		if (info->memory_range[i].type != RANGE_RAM)
			continue;
		mstart = info->memory_range[i].start;
		mend = info->memory_range[i].end;
		if (i < info->memory_ranges - 1
		    && mend == info->memory_range[i+1].start
		    && info->memory_range[i+1].type == RANGE_RAM)
			mend = info->memory_range[i+1].end;

		/* Check to see if we are fully contained */
		if ((mstart <= sstart) && (mend >= send)) {
			return 1;
		}
	}
	return 0;
}

static int valid_memory_segment(struct kexec_info *info,
				struct kexec_segment *segment)
{
	unsigned long sstart, send;
	sstart = (unsigned long)segment->mem;
	send   = sstart + segment->memsz - 1;

	return valid_memory_range(info, sstart, send);
}

int sort_segments(struct kexec_info *info)
{
	int i, j;
	void *end;

	/* Do a stupid insertion sort... */
	for (i = 0; i < info->nr_segments; i++) {
		int tidx;
		struct kexec_segment temp;
		tidx = i;
		for (j = i +1; j < info->nr_segments; j++) {
			if (info->segment[j].mem < info->segment[tidx].mem) {
				tidx = j;
			}
		}
		if (tidx != i) {
			temp = info->segment[tidx];
			info->segment[tidx] = info->segment[i];
			info->segment[i] = temp;
		}
	}
	/* Now see if any of the segments overlap */
	end = 0;
	for (i = 0; i < info->nr_segments; i++) {
		if (end > info->segment[i].mem) {
			ERROR("overlapping memory segments at %p\n",end);
			return -1;
		}
		end = ((char *)info->segment[i].mem) + info->segment[i].memsz;
	}
	return 0;
}


uint16_t elf16_to_cpu(const struct mem_ehdr *ehdr, uint16_t value)
{
	if (ehdr->ei_data == ELFDATA2LSB) {
		value = le16_to_cpu(value);
	}
	else if (ehdr->ei_data == ELFDATA2MSB) {
		value = be16_to_cpu(value);
	}
	return value;
}

uint32_t elf32_to_cpu(const struct mem_ehdr *ehdr, uint32_t value)
{
	if (ehdr->ei_data == ELFDATA2LSB) {
		value = le32_to_cpu(value);
	}
	else if (ehdr->ei_data == ELFDATA2MSB) {
		value = be32_to_cpu(value);
	}
	return value;
}

uint64_t elf64_to_cpu(const struct mem_ehdr *ehdr, uint64_t value)
{
	if (ehdr->ei_data == ELFDATA2LSB) {
		value = le64_to_cpu(value);
	}
	else if (ehdr->ei_data == ELFDATA2MSB) {
		value = be64_to_cpu(value);
	}
	return value;
}

uint16_t cpu_to_elf16(const struct mem_ehdr *ehdr, uint16_t value)
{
	if (ehdr->ei_data == ELFDATA2LSB) {
		value = cpu_to_le16(value);
	}
	else if (ehdr->ei_data == ELFDATA2MSB) {
		value = cpu_to_be16(value);
	}
	return value;
}

uint32_t cpu_to_elf32(const struct mem_ehdr *ehdr, uint32_t value)
{
	if (ehdr->ei_data == ELFDATA2LSB) {
		value = cpu_to_le32(value);
	}
	else if (ehdr->ei_data == ELFDATA2MSB) {
		value = cpu_to_be32(value);
	}
	return value;
}

uint64_t cpu_to_elf64(const struct mem_ehdr *ehdr, uint64_t value)
{
	if (ehdr->ei_data == ELFDATA2LSB) {
		value = cpu_to_le64(value);
	}
	else if (ehdr->ei_data == ELFDATA2MSB) {
		value = cpu_to_be64(value);
	}
	return value;
}

#define ELF32_MAX 0xffffffff
#define ELF64_MAX 0xffffffffffffffff
#if ELF64_MAX > ULONG_MAX
#undef ELF64_MAX
#define ELF64_MAX ULONG_MAX
#endif

unsigned long elf_max_addr(const struct mem_ehdr *ehdr)
{
	unsigned long max_addr = 0;
	if (ehdr->ei_class == ELFCLASS32) {
		max_addr = ELF32_MAX;
	}
	else if (ehdr->ei_class == ELFCLASS64) {
		max_addr = ELF64_MAX;
	}
	return max_addr;
}
static int build_mem_elf32_ehdr(const char *buf, off_t len, struct mem_ehdr *ehdr)
{
	Elf32_Ehdr lehdr;
	if ((uintmax_t)len < (uintmax_t)sizeof(lehdr)) {
		/* Buffer is to small to be an elf executable */
		DEBUG("Buffer is to small to hold ELF header\n");
		return -1;
	}
	memcpy(&lehdr, buf, sizeof(lehdr));
	if (elf16_to_cpu(ehdr, lehdr.e_ehsize) != sizeof(Elf32_Ehdr)) {
		/* Invalid Elf header size */
		DEBUG("Bad ELF header size\n");
		return -1;
	}
	if (elf32_to_cpu(ehdr, lehdr.e_entry) > UINT32_MAX) {
		/* entry is to large */
		DEBUG("ELF e_entry to large\n");
		return -1;
	}
	if (elf32_to_cpu(ehdr, lehdr.e_phoff) > UINT32_MAX) {
		/* phoff is to large */
		DEBUG("ELF e_phoff to large\n");
		return -1;
	}
	if (elf32_to_cpu(ehdr, lehdr.e_shoff) > UINT32_MAX) {
		/* shoff is to large */
		DEBUG("ELF e_shoff to large\n");
		return -1;
	}
	ehdr->e_type      = elf16_to_cpu(ehdr, lehdr.e_type);
	ehdr->e_machine   = elf16_to_cpu(ehdr, lehdr.e_machine);
	ehdr->e_version   = elf32_to_cpu(ehdr, lehdr.e_version);
	ehdr->e_entry     = elf32_to_cpu(ehdr, lehdr.e_entry);
	ehdr->e_phoff     = elf32_to_cpu(ehdr, lehdr.e_phoff);
	ehdr->e_shoff     = elf32_to_cpu(ehdr, lehdr.e_shoff);
	ehdr->e_flags     = elf32_to_cpu(ehdr, lehdr.e_flags);
	ehdr->e_phnum     = elf16_to_cpu(ehdr, lehdr.e_phnum);
	ehdr->e_shnum     = elf16_to_cpu(ehdr, lehdr.e_shnum);
	ehdr->e_shstrndx  = elf16_to_cpu(ehdr, lehdr.e_shstrndx);

	if ((ehdr->e_phnum > 0) &&
		(elf16_to_cpu(ehdr, lehdr.e_phentsize) != sizeof(Elf32_Phdr)))
	{
		/* Invalid program header size */
		DEBUG("ELF bad program header size\n");
		return -1;
	}
	if ((ehdr->e_shnum > 0) &&
		(elf16_to_cpu(ehdr, lehdr.e_shentsize) != sizeof(Elf32_Shdr)))
	{
		/* Invalid section header size */
		DEBUG("ELF bad section header size\n");
		return -1;
	}

	return 0;
}

static int build_mem_elf64_ehdr(const char *buf, off_t len, struct mem_ehdr *ehdr)
{
	Elf64_Ehdr lehdr;
	if ((uintmax_t)len < (uintmax_t)sizeof(lehdr)) {
		/* Buffer is to small to be an elf executable */
		DEBUG("Buffer is to small to hold ELF header\n");
		return -1;
	}
	memcpy(&lehdr, buf, sizeof(lehdr));
	if (elf16_to_cpu(ehdr, lehdr.e_ehsize) != sizeof(Elf64_Ehdr)) {
		/* Invalid Elf header size */
		DEBUG("Bad ELF header size\n");
		return -1;
	}
	if (elf32_to_cpu(ehdr, lehdr.e_entry) > UINT32_MAX) {
		/* entry is to large */
		DEBUG("ELF e_entry to large\n");
		return -1;
	}
	if (elf32_to_cpu(ehdr, lehdr.e_phoff) > UINT32_MAX) {
		/* phoff is to large */
		DEBUG("ELF e_phoff to large\n");
		return -1;
	}
	if (elf32_to_cpu(ehdr, lehdr.e_shoff) > UINT32_MAX) {
		/* shoff is to large */
		DEBUG("ELF e_shoff to large\n");
		return -1;
	}
	ehdr->e_type      = elf16_to_cpu(ehdr, lehdr.e_type);
	ehdr->e_machine   = elf16_to_cpu(ehdr, lehdr.e_machine);
	ehdr->e_version   = elf32_to_cpu(ehdr, lehdr.e_version);
	ehdr->e_entry     = elf64_to_cpu(ehdr, lehdr.e_entry);
	ehdr->e_phoff     = elf64_to_cpu(ehdr, lehdr.e_phoff);
	ehdr->e_shoff     = elf64_to_cpu(ehdr, lehdr.e_shoff);
	ehdr->e_flags     = elf32_to_cpu(ehdr, lehdr.e_flags);
	ehdr->e_phnum     = elf16_to_cpu(ehdr, lehdr.e_phnum);
	ehdr->e_shnum     = elf16_to_cpu(ehdr, lehdr.e_shnum);
	ehdr->e_shstrndx  = elf16_to_cpu(ehdr, lehdr.e_shstrndx);

	if ((ehdr->e_phnum > 0) &&
		(elf16_to_cpu(ehdr, lehdr.e_phentsize) != sizeof(Elf64_Phdr)))
	{
		/* Invalid program header size */
		DEBUG("ELF bad program header size\n");
		return -1;
	}
	if ((ehdr->e_shnum > 0) &&
		(elf16_to_cpu(ehdr, lehdr.e_shentsize) != sizeof(Elf64_Shdr)))
	{
		/* Invalid section header size */
		DEBUG("ELF bad section header size\n");
		return -1;
	}

	return 0;
}

static int build_mem_ehdr(const char *buf, off_t len, struct mem_ehdr *ehdr)
{
	unsigned char e_ident[EI_NIDENT];
	int result;
	memset(ehdr, 0, sizeof(*ehdr));
	if ((uintmax_t)len < (uintmax_t)sizeof(e_ident)) {
		/* Buffer is to small to be an elf executable */
		DEBUG("Buffer is to small to hold ELF e_ident\n");
		return -1;
	}
	memcpy(e_ident, buf, sizeof(e_ident));
	if (memcmp(e_ident, ELFMAG, SELFMAG) != 0) {
		/* No ELF header magic */
		DEBUG("NO ELF header magic\n");
		return -1;
	}
	ehdr->ei_class   = e_ident[EI_CLASS];
	ehdr->ei_data    = e_ident[EI_DATA];
	if (	(ehdr->ei_class != ELFCLASS32) &&
		(ehdr->ei_class != ELFCLASS64))
	{
		/* Not a supported elf class */
		DEBUG("Not a supported ELF class\n");
		return -1;
	}
	if (	(ehdr->ei_data != ELFDATA2LSB) &&
		(ehdr->ei_data != ELFDATA2MSB))
	{
		/* Not a supported elf data type */
		DEBUG("Not a supported ELF data format\n");
		return -1;
	}

	result = -1;
	if (ehdr->ei_class == ELFCLASS32) {
		result = build_mem_elf32_ehdr(buf, len, ehdr);
	}
	else if (ehdr->ei_class == ELFCLASS64) {
		result = build_mem_elf64_ehdr(buf, len, ehdr);
	}
	if (result < 0) {
		return result;
	}
	if ((e_ident[EI_VERSION] != EV_CURRENT) ||
		(ehdr->e_version != EV_CURRENT))
	{
		DEBUG("Unknown ELF version\n");
		/* Unknwon elf version */
		return -1;
	}
	return 0;
}

static int build_mem_elf32_phdr(const char *buf, struct mem_ehdr *ehdr, int idx)
{
	struct mem_phdr *phdr;
	const char *pbuf;
	Elf32_Phdr lphdr;
	pbuf = buf + ehdr->e_phoff + (idx * sizeof(lphdr));
	phdr = &ehdr->e_phdr[idx];
	memcpy(&lphdr, pbuf, sizeof(lphdr));

	if (	(elf32_to_cpu(ehdr, lphdr.p_filesz) > UINT32_MAX) ||
		(elf32_to_cpu(ehdr, lphdr.p_memsz)  > UINT32_MAX) ||
		(elf32_to_cpu(ehdr, lphdr.p_offset) > UINT32_MAX) ||
		(elf32_to_cpu(ehdr, lphdr.p_paddr)  > UINT32_MAX) ||
		(elf32_to_cpu(ehdr, lphdr.p_vaddr)  > UINT32_MAX) ||
		(elf32_to_cpu(ehdr, lphdr.p_align)  > UINT32_MAX))
	{
		fprintf(stderr, "Program segment size out of range\n");
		return -1;
	}

	phdr->p_type   = elf32_to_cpu(ehdr, lphdr.p_type);
	phdr->p_paddr  = elf32_to_cpu(ehdr, lphdr.p_paddr);
	phdr->p_vaddr  = elf32_to_cpu(ehdr, lphdr.p_vaddr);
	phdr->p_filesz = elf32_to_cpu(ehdr, lphdr.p_filesz);
	phdr->p_memsz  = elf32_to_cpu(ehdr, lphdr.p_memsz);
	phdr->p_offset = elf32_to_cpu(ehdr, lphdr.p_offset);
	phdr->p_flags  = elf32_to_cpu(ehdr, lphdr.p_flags);
	phdr->p_align  = elf32_to_cpu(ehdr, lphdr.p_align);

	return 0;
}

static int build_mem_elf64_phdr(const char *buf, struct mem_ehdr *ehdr, int idx)
{
	struct mem_phdr *phdr;
	const char *pbuf;
	Elf64_Phdr lphdr;
	pbuf = buf + ehdr->e_phoff + (idx * sizeof(lphdr));
	phdr = &ehdr->e_phdr[idx];
	memcpy(&lphdr, pbuf, sizeof(lphdr));

	if (	(elf64_to_cpu(ehdr, lphdr.p_filesz) > UINT64_MAX) ||
		(elf64_to_cpu(ehdr, lphdr.p_memsz)  > UINT64_MAX) ||
		(elf64_to_cpu(ehdr, lphdr.p_offset) > UINT64_MAX) ||
		(elf64_to_cpu(ehdr, lphdr.p_paddr)  > UINT64_MAX) ||
		(elf64_to_cpu(ehdr, lphdr.p_vaddr)  > UINT64_MAX) ||
		(elf64_to_cpu(ehdr, lphdr.p_align)  > UINT64_MAX))
	{
		ERROR("program segment size out of range\n");
		return -1;
	}

	phdr->p_type   = elf32_to_cpu(ehdr, lphdr.p_type);
	phdr->p_paddr  = elf64_to_cpu(ehdr, lphdr.p_paddr);
	phdr->p_vaddr  = elf64_to_cpu(ehdr, lphdr.p_vaddr);
	phdr->p_filesz = elf64_to_cpu(ehdr, lphdr.p_filesz);
	phdr->p_memsz  = elf64_to_cpu(ehdr, lphdr.p_memsz);
	phdr->p_offset = elf64_to_cpu(ehdr, lphdr.p_offset);
	phdr->p_flags  = elf32_to_cpu(ehdr, lphdr.p_flags);
	phdr->p_align  = elf64_to_cpu(ehdr, lphdr.p_align);

	return 0;
}

static int build_mem_phdrs(const char *buf, off_t len, struct mem_ehdr *ehdr,
				uint32_t flags)
{
	size_t phdr_size, mem_phdr_size, i;

	/* e_phnum is at most 65535 so calculating
	 * the size of the program header cannot overflow.
	 */
	/* Is the program header in the file buffer? */
	phdr_size = 0;
	if (ehdr->ei_class == ELFCLASS32) {
		phdr_size = sizeof(Elf32_Phdr);
	}
	else if (ehdr->ei_class == ELFCLASS64) {
		phdr_size = sizeof(Elf64_Phdr);
	}
	else {
		ERROR("invalid ei_class?\n");
		return -1;
	}
	phdr_size *= ehdr->e_phnum;
	if ((uintmax_t)(ehdr->e_phoff + phdr_size) > (uintmax_t)len) {
		/* The program header did not fit in the file buffer */
		DEBUG("ELF program segment truncated\n");
		return -1;
	}

	/* Allocate the e_phdr array */
	mem_phdr_size = sizeof(ehdr->e_phdr[0]) * ehdr->e_phnum;
	ehdr->e_phdr = malloc(mem_phdr_size);

	if(!ehdr->e_phdr)
	{
		FATAL("malloc - %s\n",strerror(errno));
		return -1;
	}

	for(i = 0; i < ehdr->e_phnum; i++) {
		struct mem_phdr *phdr;
		int result;
		result = -1;
		if (ehdr->ei_class == ELFCLASS32) {
			result = build_mem_elf32_phdr(buf, ehdr, i);

		}
		else if (ehdr->ei_class == ELFCLASS64) {
			result = build_mem_elf64_phdr(buf, ehdr, i);
		}
		if (result < 0) {
			free(ehdr->e_phdr);
			return result;
		}

		/* Check the program headers to be certain
		 * they are safe to use.
		 * Skip the check if ELF_SKIP_FILESZ_CHECK is set.
		 */
		phdr = &ehdr->e_phdr[i];
		if (!(flags & ELF_SKIP_FILESZ_CHECK)
			&& (uintmax_t)(phdr->p_offset + phdr->p_filesz) >
			   (uintmax_t)len) {
			/* The segment does not fit in the buffer */
			DEBUG("ELF segment not in file\n");
			free(ehdr->e_phdr);
			return -1;
		}
		if ((phdr->p_paddr + phdr->p_memsz) < phdr->p_paddr) {
			/* The memory address wraps */
			DEBUG("ELF address wrap around\n");
			free(ehdr->e_phdr);
			return -1;
		}
		/* Remember where the segment lives in the buffer */
		phdr->p_data = buf + phdr->p_offset;
	}
	return 0;
}

static int build_mem_elf32_shdr(const char *buf, struct mem_ehdr *ehdr, int idx)
{
	struct mem_shdr *shdr;
	const char *sbuf;
	int size_ok;
	Elf32_Shdr lshdr;
	sbuf = buf + ehdr->e_shoff + (idx * sizeof(lshdr));
	shdr = &ehdr->e_shdr[idx];
	memcpy(&lshdr, sbuf, sizeof(lshdr));

	if (	(elf32_to_cpu(ehdr, lshdr.sh_flags)     > UINT32_MAX) ||
		(elf32_to_cpu(ehdr, lshdr.sh_addr)      > UINT32_MAX) ||
		(elf32_to_cpu(ehdr, lshdr.sh_offset)    > UINT32_MAX) ||
		(elf32_to_cpu(ehdr, lshdr.sh_size)      > UINT32_MAX) ||
		(elf32_to_cpu(ehdr, lshdr.sh_addralign) > UINT32_MAX) ||
		(elf32_to_cpu(ehdr, lshdr.sh_entsize)   > UINT32_MAX))
	{
		ERROR("program section size out of range\n");
		return -1;
	}

	shdr->sh_name      = elf32_to_cpu(ehdr, lshdr.sh_name);
	shdr->sh_type      = elf32_to_cpu(ehdr, lshdr.sh_type);
	shdr->sh_flags     = elf32_to_cpu(ehdr, lshdr.sh_flags);
	shdr->sh_addr      = elf32_to_cpu(ehdr, lshdr.sh_addr);
	shdr->sh_offset    = elf32_to_cpu(ehdr, lshdr.sh_offset);
	shdr->sh_size      = elf32_to_cpu(ehdr, lshdr.sh_size);
	shdr->sh_link      = elf32_to_cpu(ehdr, lshdr.sh_link);
	shdr->sh_info      = elf32_to_cpu(ehdr, lshdr.sh_info);
	shdr->sh_addralign = elf32_to_cpu(ehdr, lshdr.sh_addralign);
	shdr->sh_entsize   = elf32_to_cpu(ehdr, lshdr.sh_entsize);

	/* Now verify sh_entsize */
	size_ok = 0;
	switch(shdr->sh_type) {
	case SHT_SYMTAB:
		size_ok = shdr->sh_entsize == sizeof(Elf32_Sym);
		break;
	case SHT_RELA:
		size_ok = shdr->sh_entsize == sizeof(Elf32_Rela);
		break;
	case SHT_DYNAMIC:
		size_ok = shdr->sh_entsize == sizeof(Elf32_Dyn);
		break;
	case SHT_REL:
		size_ok = shdr->sh_entsize == sizeof(Elf32_Rel);
		break;
	case SHT_NOTE:
	case SHT_NULL:
	case SHT_PROGBITS:
	case SHT_HASH:
	case SHT_NOBITS:
	default:
		/* This is a section whose entsize requirements
		 * I don't care about.  If I don't know about
		 * the section I can't care about it's entsize
		 * requirements.
		 */
		size_ok = 1;
		break;
	}
	if (!size_ok) {
		ERROR("bad section header(%x) entsize: %ld\n",shdr->sh_type, shdr->sh_entsize);
		return -1;
	}
	return 0;
}

static int build_mem_elf64_shdr(const char *buf, struct mem_ehdr *ehdr, int idx)
{
	struct mem_shdr *shdr;
	const char *sbuf;
	int size_ok;
	Elf64_Shdr lshdr;
	sbuf = buf + ehdr->e_shoff + (idx * sizeof(lshdr));
	shdr = &ehdr->e_shdr[idx];
	memcpy(&lshdr, sbuf, sizeof(lshdr));

	if (	(elf64_to_cpu(ehdr, lshdr.sh_flags)     > UINT64_MAX) ||
		(elf64_to_cpu(ehdr, lshdr.sh_addr)      > UINT64_MAX) ||
		(elf64_to_cpu(ehdr, lshdr.sh_offset)    > UINT64_MAX) ||
		(elf64_to_cpu(ehdr, lshdr.sh_size)      > UINT64_MAX) ||
		(elf64_to_cpu(ehdr, lshdr.sh_addralign) > UINT64_MAX) ||
		(elf64_to_cpu(ehdr, lshdr.sh_entsize)   > UINT64_MAX))
	{
		ERROR("program section size out of range\n");
		return -1;
	}

	shdr->sh_name      = elf32_to_cpu(ehdr, lshdr.sh_name);
	shdr->sh_type      = elf32_to_cpu(ehdr, lshdr.sh_type);
	shdr->sh_flags     = elf64_to_cpu(ehdr, lshdr.sh_flags);
	shdr->sh_addr      = elf64_to_cpu(ehdr, lshdr.sh_addr);
	shdr->sh_offset    = elf64_to_cpu(ehdr, lshdr.sh_offset);
	shdr->sh_size      = elf64_to_cpu(ehdr, lshdr.sh_size);
	shdr->sh_link      = elf32_to_cpu(ehdr, lshdr.sh_link);
	shdr->sh_info      = elf32_to_cpu(ehdr, lshdr.sh_info);
	shdr->sh_addralign = elf64_to_cpu(ehdr, lshdr.sh_addralign);
	shdr->sh_entsize   = elf64_to_cpu(ehdr, lshdr.sh_entsize);

	/* Now verify sh_entsize */
	size_ok = 0;
	switch(shdr->sh_type) {
	case SHT_SYMTAB:
		size_ok = shdr->sh_entsize == sizeof(Elf64_Sym);
		break;
	case SHT_RELA:
		size_ok = shdr->sh_entsize == sizeof(Elf64_Rela);
		break;
	case SHT_DYNAMIC:
		size_ok = shdr->sh_entsize == sizeof(Elf64_Dyn);
		break;
	case SHT_REL:
		size_ok = shdr->sh_entsize == sizeof(Elf64_Rel);
		break;
	case SHT_NOTE:
	case SHT_NULL:
	case SHT_PROGBITS:
	case SHT_HASH:
	case SHT_NOBITS:
	default:
		/* This is a section whose entsize requirements
		 * I don't care about.  If I don't know about
		 * the section I can't care about it's entsize
		 * requirements.
		 */
		size_ok = 1;
		break;
	}
	if (!size_ok) {
		ERROR("bad section header(%x) entsize: %ld\n",shdr->sh_type, shdr->sh_entsize);
		return -1;
	}
	return 0;
}

static int build_mem_shdrs(const char *buf, off_t len, struct mem_ehdr *ehdr,
				uint32_t flags)
{
	size_t shdr_size, mem_shdr_size, i;

	/* e_shnum is at most 65536 so calculating
	 * the size of the section header cannot overflow.
	 */
	/* Is the program header in the file buffer? */
	shdr_size = 0;
	if (ehdr->ei_class == ELFCLASS32) {
		shdr_size = sizeof(Elf32_Shdr);
	}
	else if (ehdr->ei_class == ELFCLASS64) {
		shdr_size = sizeof(Elf64_Shdr);
	}
	else {
		ERROR("invalid ei_class?\n");
		return -1;
	}
	shdr_size *= ehdr->e_shnum;
	if ((uintmax_t)(ehdr->e_shoff + shdr_size) > (uintmax_t)len) {
		/* The section header did not fit in the file buffer */
		DEBUG("ELF section header does not fit in file\n");
		return -1;
	}

	/* Allocate the e_shdr array */
	mem_shdr_size = sizeof(ehdr->e_shdr[0]) * ehdr->e_shnum;
	ehdr->e_shdr = malloc(mem_shdr_size);

	if(!ehdr->e_shdr)
	{
		FATAL("malloc - %s\n",strerror(errno));
		return -1;
	}

	for(i = 0; i < ehdr->e_shnum; i++) {
		struct mem_shdr *shdr;
		int result;
		result = -1;
		if (ehdr->ei_class == ELFCLASS32) {
			result = build_mem_elf32_shdr(buf, ehdr, i);
		}
		else if (ehdr->ei_class == ELFCLASS64) {
			result = build_mem_elf64_shdr(buf, ehdr, i);
		}
		if (result < 0) {
			free(ehdr->e_shdr);
			return result;
		}
		/* Check the section headers to be certain
		 * they are safe to use.
		 * Skip the check if ELF_SKIP_FILESZ_CHECK is set.
		 */
		shdr = &ehdr->e_shdr[i];
		if (!(flags & ELF_SKIP_FILESZ_CHECK)
			&& (shdr->sh_type != SHT_NOBITS)
			&& (uintmax_t)(shdr->sh_offset + shdr->sh_size) >
			   (uintmax_t)len) {
			/* The section does not fit in the buffer */
			DEBUG("ELF section %zd not in file\n",i);
			free(ehdr->e_shdr);
			return -1;
		}
		if ((shdr->sh_addr + shdr->sh_size) < shdr->sh_addr) {
			/* The memory address wraps */
			DEBUG("ELF address wrap around\n");
			free(ehdr->e_shdr);
			return -1;
		}
		/* Remember where the section lives in the buffer */
		shdr->sh_data = (unsigned char *)(buf + shdr->sh_offset);
	}
	return 0;
}

static void read_nhdr(const struct mem_ehdr *ehdr,
	ElfNN_Nhdr *hdr, const unsigned char *note)
{
	memcpy(hdr, note, sizeof(*hdr));
	hdr->n_namesz = elf32_to_cpu(ehdr, hdr->n_namesz);
	hdr->n_descsz = elf32_to_cpu(ehdr, hdr->n_descsz);
	hdr->n_type   = elf32_to_cpu(ehdr, hdr->n_type);

}
static int build_mem_notes(struct mem_ehdr *ehdr)
{
	const unsigned char *note_start, *note_end, *note;
	size_t note_size, i;
	/* First find the note segment or section */
	note_start = note_end = NULL;
	for(i = 0; !note_start && (i < ehdr->e_phnum); i++) {
		struct mem_phdr *phdr = &ehdr->e_phdr[i];
		/*
		 * binutils <= 2.17 has a bug where it can create the
		 * PT_NOTE segment with an offset of 0. Therefore
		 * check p_offset > 0.
		 *
		 * See: http://sourceware.org/bugzilla/show_bug.cgi?id=594
		 */
		if (phdr->p_type == PT_NOTE && phdr->p_offset) {
			note_start = (unsigned char *)phdr->p_data;
			note_end = note_start + phdr->p_filesz;
		}
	}
	for(i = 0; !note_start && (i < ehdr->e_shnum); i++) {
		struct mem_shdr *shdr = &ehdr->e_shdr[i];
		if (shdr->sh_type == SHT_NOTE) {
			note_start = shdr->sh_data;
			note_end = note_start + shdr->sh_size;
		}
	}
	if (!note_start) {
		return 0;
	}

	/* Walk through and count the notes */
	ehdr->e_notenum = 0;
	for(note = note_start; note < note_end; note+= note_size) {
		ElfNN_Nhdr hdr;
		read_nhdr(ehdr, &hdr, note);
		note_size  = sizeof(hdr);
		note_size += (hdr.n_namesz + 3) & ~3;
		note_size += (hdr.n_descsz + 3) & ~3;
		ehdr->e_notenum += 1;
	}
	/* Now walk and normalize the notes */
	ehdr->e_note = malloc(sizeof(*ehdr->e_note) * ehdr->e_notenum);

	if(!ehdr->e_note)
	{
		FATAL("malloc - %s\n",strerror(errno));
		return -1;
	}

	for(i = 0, note = note_start; note < note_end; note+= note_size, i++) {
		const unsigned char *name, *desc;
		ElfNN_Nhdr hdr;
		read_nhdr(ehdr, &hdr, note);
		note_size  = sizeof(hdr);
		name       = note + note_size;
		note_size += (hdr.n_namesz + 3) & ~3;
		desc       = note + note_size;
		note_size += (hdr.n_descsz + 3) & ~3;

		if ((hdr.n_namesz != 0) && (name[hdr.n_namesz -1] != '\0')) {
			/* If note name string is not null terminated, just
			 * warn user about it and continue processing. This
			 * allows us to parse /proc/kcore on older kernels
			 * where /proc/kcore elf notes were not null
			 * terminated. It has been fixed in 2.6.19.
			 */
			WARN("Elf Note name is not null terminated\n");
		}
		ehdr->e_note[i].n_type = hdr.n_type;
		ehdr->e_note[i].n_name = (char *)name;
		ehdr->e_note[i].n_desc = desc;
		ehdr->e_note[i].n_descsz = hdr.n_descsz;

	}
	return 0;
}

void free_elf_info(struct mem_ehdr *ehdr)
{
	free(ehdr->e_phdr);
	free(ehdr->e_shdr);
	memset(ehdr, 0, sizeof(*ehdr));
}

int build_elf_info(const char *buf, off_t len, struct mem_ehdr *ehdr,
			uint32_t flags)
{
	int result;
	result = build_mem_ehdr(buf, len, ehdr);
	if (result < 0) {
		return result;
	}
	if ((ehdr->e_phoff > 0) && (ehdr->e_phnum > 0)) {
		result = build_mem_phdrs(buf, len, ehdr, flags);
		if (result < 0) {
			free_elf_info(ehdr);
			return result;
		}
	}
	if ((ehdr->e_shoff > 0) && (ehdr->e_shnum > 0)) {
		result = build_mem_shdrs(buf, len, ehdr, flags);
		if (result < 0) {
			free_elf_info(ehdr);
			return result;
		}
	}
	result = build_mem_notes(ehdr);
	if (result < 0) {
		free_elf_info(ehdr);
		return result;
	}
	return 0;
}

static struct mem_sym elf_sym(struct mem_ehdr *ehdr, const unsigned char *ptr)
{
	struct mem_sym sym = { 0, 0, 0, 0, 0, 0 };
	if (ehdr->ei_class == ELFCLASS32) {
		Elf32_Sym lsym;
		memcpy(&lsym, ptr, sizeof(lsym));
		sym.st_name  = elf32_to_cpu(ehdr, lsym.st_name);
		sym.st_value = elf32_to_cpu(ehdr, lsym.st_value);
		sym.st_size  = elf32_to_cpu(ehdr, lsym.st_size);
		sym.st_info  = lsym.st_info;
		sym.st_other = lsym.st_other;
		sym.st_shndx = elf16_to_cpu(ehdr, lsym.st_shndx);
	}
	else if (ehdr->ei_class == ELFCLASS64) {
		Elf64_Sym lsym;
		memcpy(&lsym, ptr, sizeof(lsym));
		sym.st_name  = elf32_to_cpu(ehdr, lsym.st_name);
		sym.st_value = elf64_to_cpu(ehdr, lsym.st_value);
		sym.st_size  = elf64_to_cpu(ehdr, lsym.st_size);
		sym.st_info  = lsym.st_info;
		sym.st_other = lsym.st_other;
		sym.st_shndx = elf16_to_cpu(ehdr, lsym.st_shndx);
	}
	else {
		ERROR("Bad elf class");
	}
	return sym;
}

static size_t elf_sym_size(struct mem_ehdr *ehdr)
{
	size_t sym_size = 0;
	if (ehdr->ei_class == ELFCLASS32) {
		sym_size = sizeof(Elf32_Sym);
	}
	else if (ehdr->ei_class == ELFCLASS64) {
		sym_size = sizeof(Elf64_Sym);
	}
	else {
		ERROR("Bad elf class");
	}
	return sym_size;
}

int elf_rel_find_symbol(struct mem_ehdr *ehdr,
	const char *name, struct mem_sym *ret_sym)
{
	struct mem_shdr *shdr, *shdr_end;

	if (!ehdr->e_shdr) {
		/* "No section header? */
		return  -1;
	}
	/* Walk through the sections and find the symbol table */
	shdr_end = &ehdr->e_shdr[ehdr->e_shnum];
	for (shdr = ehdr->e_shdr; shdr != shdr_end; shdr++) {
		const char *strtab;
		size_t sym_size;
		const unsigned char *ptr, *sym_end;
		if (shdr->sh_type != SHT_SYMTAB) {
			continue;
		}
		if (shdr->sh_link > ehdr->e_shnum) {
			/* Invalid strtab section number? */
			continue;
		}
		strtab = (char *)ehdr->e_shdr[shdr->sh_link].sh_data;
		/* Walk through the symbol table and find the symbol */
		sym_size = elf_sym_size(ehdr);
		if(!sym_size)
			return -1;
		sym_end = shdr->sh_data + shdr->sh_size;
		for(ptr = shdr->sh_data; ptr < sym_end; ptr += sym_size) {
			struct mem_sym sym;
			sym = elf_sym(ehdr, ptr);
			if(!sym.st_name && !sym.st_info && !sym.st_other && !sym.st_shndx && !sym.st_value && !sym.st_size)
			{
				ERROR("cannot get ELF symbol\n");
				return -1;
			}

			if (ELF32_ST_BIND(sym.st_info) != STB_GLOBAL) {
				continue;
			}
			if (strcmp(strtab + sym.st_name, name) != 0) {
				continue;
			}
			if ((sym.st_shndx == STN_UNDEF) ||
				(sym.st_shndx > ehdr->e_shnum))
			{
				ERROR("Symbol: %s has Bad section index %lu\n",name, sym.st_shndx);
				return -1;
			}
			*ret_sym = sym;
			return 0;
		}
	}
	/* I did not find it :( */
	return -1;

}

int elf_rel_set_symbol(struct mem_ehdr *ehdr,
	const char *name, const void *buf, size_t size)
{
	unsigned char *sym_buf;
	struct mem_shdr *shdr;
	struct mem_sym sym;
	int result;

	result = elf_rel_find_symbol(ehdr, name, &sym);
	if (result < 0) {
		ERROR("Symbol: %s not found cannot set\n",name);
		return -1;
	}
	if (sym.st_size != size) {
		ERROR("Symbol: %s has size: %ld not %d\n",name, sym.st_size, size);
		return -1;
	}
	shdr = &ehdr->e_shdr[sym.st_shndx];
	if (shdr->sh_type == SHT_NOBITS) {
		ERROR("Symbol: %s is in a bss section cannot set\n", name);
		return -1;
	}
	sym_buf = (unsigned char *)(shdr->sh_data + sym.st_value);
	memcpy(sym_buf, buf, size);
	return 0;
}

static int update_purgatory(struct kexec_info *info)
{
	static const uint8_t null_buf[256];
	sha256_context ctx;
	sha256_digest_t digest;
	struct sha256_region region[SHA256_REGIONS];
	int i, j;
	/* Don't do anything if we are not using purgatory */
	if (!info->rhdr.e_shdr) {
		return 0;
	}
	memset(region, 0, sizeof(region));
	sha256_starts(&ctx);
	/* Compute a hash of the loaded kernel */
	for(j = i = 0; i < info->nr_segments; i++) {
		unsigned long nullsz;
		/* Don't include purgatory in the checksum.  The stack
		 * in the bss will definitely change, and the .data section
		 * will also change when we poke the sha256_digest in there.
		 * A very clever/careful person could probably improve this.
		 */
		if (info->segment[i].mem == (void *)info->rhdr.rel_addr) {
			continue;
		}
		sha256_update(&ctx, info->segment[i].buf,
			      info->segment[i].bufsz);
		nullsz = info->segment[i].memsz - info->segment[i].bufsz;
		while(nullsz) {
			unsigned long bytes = nullsz;
			if (bytes > sizeof(null_buf)) {
				bytes = sizeof(null_buf);
			}
			sha256_update(&ctx, null_buf, bytes);
			nullsz -= bytes;
		}
		region[j].start = (unsigned long) info->segment[i].mem;
		region[j].len   = info->segment[i].memsz;
		j++;
	}
	sha256_finish(&ctx, digest);
	if(elf_rel_set_symbol(&info->rhdr, "sha256_regions", &region,sizeof(region)))
		return -1;
	if(elf_rel_set_symbol(&info->rhdr, "sha256_digest", &digest,sizeof(digest)))
		return -1;
	return 0;
}

static inline long kexec_load(void *entry, unsigned long nr_segments,
			struct kexec_segment *segments, unsigned long flags)
{
	return (long) syscall(__NR_kexec_load, entry, nr_segments, segments, flags);
}

int k_load(char *kernel,char *initrd,char *cmdline)
{
	char *kernel_buf;
	off_t kernel_size;
	int result,i;
	struct kexec_info info;

	memset(&info, 0, sizeof(info));
	info.segment = NULL;
	info.nr_segments = 0;
	info.backup_start = 0;
	info.kexec_flags = KEXEC_FLAGS;

	mem_max = ULONG_MAX;
	mem_min = 0xA0000000;

	result = 0;
	/* slurp in the input kernel */
	kernel_buf = slurp_decompress_file(kernel, &kernel_size);

	if(!kernel_buf)
		return -1;

	if (get_memory_ranges(&info.memory_range, &info.memory_ranges)) {
		ERROR("could not get memory layout\n");
		free(kernel_buf);
		return -1;
	}
	if(uImage_probe(kernel_buf, kernel_size,IH_ARCH_ARM))
	{
		// NOT uImage
		if(zImage_arm_load(kernel_buf,cmdline,initrd,kernel_size, &info))
		{
			ERROR("cannot load \"%s\"\n",kernel);
			free(kernel_buf);
			return -1;
		}
	}
	else if(uImage_load(kernel_buf,cmdline,initrd,kernel_size, &info))
	{
		ERROR("cannot load \"%s\"\n",kernel);
		free(kernel_buf);
		return -1;
	}

	/* Verify all of the segments load to a valid location in memory */
	for (i = 0; i < info.nr_segments; i++) {
		if (!valid_memory_segment(&info, info.segment +i)) {
			ERROR("invalid memory segment %p - %p\n",
				info.segment[i].mem,
				((char *)info.segment[i].mem) +
				info.segment[i].memsz);
			free(kernel_buf);
			for(i=0;i<info.nr_segments;i++)
				if(info.segment[i].buf)
					free((void *)info.segment[i].buf);
			return -1;
		}
	}
	/* Sort the segments and verify we don't have overlaps */
	if (sort_segments(&info) < 0) {
		free(kernel_buf);
		for(i=0;i<info.nr_segments;i++)
			if(info.segment[i].buf)
				free((void *)info.segment[i].buf);
		return -1;
	}
	/* if purgatory is loaded update it */
	if(update_purgatory(&info))
	{
		ERROR("cannot update purgatory\n");
		return -1;
	}
	result = kexec_load(info.entry, info.nr_segments, info.segment, info.kexec_flags);
	if (result != 0)
	{
		ERROR("kexec_load failed: %s\n", strerror(errno));
		DEBUG("entry       = %p flags = %lx\n", info.entry, info.kexec_flags);
	}
	return result;
}

static inline long kexec_reboot(void)
{
	return (long) syscall(__NR_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_KEXEC, 0);
}

void k_exec(void)
{
	int returned;
	returned = kexec_reboot();
	ERROR("syscall failed: returned value = %d - %s\n", returned, strerror(errno));
}
