/*
 * Copyright (c) 2026, asprintf
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* load.c - find a kernel in the file system and start it.  The kernel is a
   flat Multiboot (version 1) image with the address fields (what
   mkimage makes of the ELF file): it is read to its load address, the
   rest of its memory is cleared, and it is entered in 32-bit protected
   mode, paging off, with %eax = 0x2badb002 and %ebx = the Multiboot
   information, as QEMU's -kernel does. */
#include "boot.h"

struct spec defspec;

#define MBI 0x70000		/* the information for the kernel: below 1 MiB, ... */
#define MBI_CMDLINE (MBI + 0x100)
#define MBI_NAME (MBI + 0x300)
#define MBI_MMAP (MBI + 0x400)
#define CMDLINE_MAX 256

static uint32_t le32(const uint8_t *p) {
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* disk<N>[s<M>]:[path] */
int spec_parse(const char *s, struct spec *sp, const struct spec *def) {
	*sp = *def;
	sp->path[0] = 0;
	const char *colon = strchr(s, ':');
	if (colon) {
		if (strncmp(s, "disk", 4))
			return -1;
		const char *p = s + 4;
		int n = 0, digits = 0, part = 0;
		for (; *p >= '0' && *p <= '9'; p++, digits++)
			n = n * 10 + *p - '0';
		if (!digits || n >= MAXDISKS)
			return -1;
		if (*p == 's') {
			for (p++, digits = 0; *p >= '0' && *p <= '9'; p++, digits++)
				part = part * 10 + *p - '0';
			if (!digits)
				return -1;
		}
		if (p != colon)
			return -1;
		sp->disk = n;
		sp->part = part;
		s = colon + 1;
	}
	if (*s) {
		if (strlen(s) + 2 > sizeof(sp->path))
			return -1;
		snprintf(sp->path, sizeof(sp->path), "%s%s", *s == '/' ? "" : "/", s);
	}
	return 0;
}

void spec_name(const struct spec *sp, char *buf, size_t n) {
	if (sp->part)
		snprintf(buf, n, "disk%ds%d", sp->disk, sp->part);
	else
		snprintf(buf, n, "disk%d", sp->disk);
}

int spec_mount(const struct spec *sp) {
	if (sp->disk < 0 || sp->disk >= ndisks || !disks[sp->disk].ok)
		return -1;
	const struct disk *dk = &disks[sp->disk];
	if (!sp->part)
		return ffs_mount(sp->disk, 0, dk->sectors);
	for (int i = 0; i < dk->nparts; i++)
		if (dk->parts[i].index == sp->part)
			return ffs_mount(sp->disk, dk->parts[i].start, dk->parts[i].sectors);
	return -1;
}

static void fail(const char *name, const char *path, const char *why) {
	printf("boot: %s%s%s: %s\n", name, path[0] ? ":" : "", path, why);
}

int boot_kernel(const struct spec *sp, const char *args) {
	char name[32];
	spec_name(sp, name, sizeof(name));
	if (spec_mount(sp)) {
		fail(name, "", "no FFS2 file system");
		return -1;
	}
	struct ffsfile f;
	if (ffs_lookup(sp->path, &f)) {
		fail(name, sp->path, "no such file");
		return -1;
	}
	if ((f.mode & 0170000) != 0100000) {
		fail(name, sp->path, "not a regular file");
		return -1;
	}
	static uint8_t hdr[8192];
	int n = ffs_read(&f, hdr, 0, sizeof(hdr));
	if (n < 0) {
		fail(name, sp->path, "read error");
		return -1;
	}
	int h;
	for (h = 0; h + 32 <= n; h += 4)
		if (le32(hdr + h) == 0x1badb002 && le32(hdr + h) + le32(hdr + h + 4) + le32(hdr + h + 8) == 0)
			break;
	if (h + 32 > n || !(le32(hdr + h + 4) & 0x10000)) {
		fail(name, sp->path, "not a Multiboot kernel");
		return -1;
	}
	uint32_t haddr = le32(hdr + h + 12), laddr = le32(hdr + h + 16), lend = le32(hdr + h + 20);
	uint32_t bend = le32(hdr + h + 24), entry = le32(hdr + h + 28);
	int bad = haddr < laddr || haddr - laddr > (uint32_t)h || laddr < 0x100000;
	uint32_t off = bad ? 0 : (uint32_t)h - (haddr - laddr);
	if (!bad && off > f.size)
		bad = 1;
	if (!bad && !lend)
		lend = laddr + (f.size - off);
	if (bad || lend < laddr || lend - laddr > f.size - off || bend < lend || entry < laddr || entry >= bend) {
		fail(name, sp->path, "bad Multiboot header");
		return -1;
	}
	if (!mem_usable(laddr, bend)) {
		fail(name, sp->path, "does not fit in memory");
		return -1;
	}
	printf("booting %s:%s: %u bytes\n", name, sp->path, lend - laddr);
	if (ffs_read(&f, (void *)laddr, off, lend - laddr) != (int)(lend - laddr)) {
		fail(name, sp->path, "read error");
		return -1;
	}
	memset((void *)lend, 0, bend - lend);

	uint32_t *mbi = (uint32_t *)MBI;
	memset(mbi, 0, 128);
	char *cmdline = (char *)MBI_CMDLINE;
	snprintf(cmdline, CMDLINE_MAX, "%s:%s%s%s", name, sp->path, args[0] ? " " : "", args);
	memcpy((void *)MBI_NAME, "V86BSD boot " VERSION, sizeof("V86BSD boot " VERSION));
	uint8_t *mm = (uint8_t *)MBI_MMAP;
	for (int i = 0; i < nmem; i++) {
		uint32_t size = 20;
		memcpy(mm, &size, 4);
		memcpy(mm + 4, &memmap[i].base, 8);
		memcpy(mm + 12, &memmap[i].len, 8);
		memcpy(mm + 20, &memmap[i].type, 4);
		mm += 24;
	}
	mbi[0] = 1 | 2 | 4 | 0x40 | 0x200;	/* memory, boot device, command line, map, loader name */
	mbi[1] = basemem;
	mbi[2] = mem_above1m() * 1024;
	mbi[3] = (uint32_t)(0x80 + sp->disk) << 24 | (uint32_t)(sp->part ? sp->part - 1 : 0xff) << 16 | 0xffff;
	mbi[4] = MBI_CMDLINE;
	mbi[11] = (uint32_t)(mm - (uint8_t *)MBI_MMAP);
	mbi[12] = MBI_MMAP;
	mbi[16] = MBI_NAME;
	__asm__ volatile("cli\n\tjmp *%%ecx" : : "a"(0x2badb002), "b"(MBI), "c"(entry) : "memory");
	for (;;)
		;
}
