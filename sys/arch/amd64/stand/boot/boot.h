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

#ifndef _BOOT_H_
#define _BOOT_H_

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#define VERSION "1.0"

/* a variable the BIOS can reach: in the first 64 KiB, see boot.ld */
#define LOW __attribute__((section(".data16"), aligned(4)))

/* a physical address as a pointer: the compiler does not see the constant, so
   it does not take an access near address zero for an error */
static inline void *phys(uintptr_t a) {
	__asm__("" : "+r"(a));
	return (void *)a;
}

#define CF 0x0001
#define ZF 0x0040
struct biosregs {
	uint32_t eax, ebx, ecx, edx, esi, edi, ebp;
	uint16_t es, flags;
};
void bioscall(int intno, struct biosregs *r);

/* lib.c */
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);
size_t strlen(const char *);
int strcmp(const char *, const char *);
int strncmp(const char *, const char *, size_t);
char *strchr(const char *, int);
int printf(const char *, ...);
int snprintf(char *, size_t, const char *, ...);
uint32_t crc32(const void *, size_t);

/* cons.c */
void cons_init(void);
void putchar(int);
int trygetchar(void);
uint32_t ticks(void);
int readline(char *, int, int);
uint8_t inb(uint16_t);
void outb(uint16_t, uint8_t);

/* mem.c: the memory the BIOS tells of */
struct mrange {
	uint64_t base, len;
	uint32_t type;
};
extern struct mrange memmap[];
extern int nmem;
extern unsigned basemem;	/* KiB below 640K */
void mem_probe(void);
int mem_usable(uint32_t lo, uint32_t hi);
uint32_t mem_above1m(void);	/* MiB */

/* disk.c: the disks of the BIOS and their partitions */
#define MAXDISKS 8
#define MAXPARTS 15
struct part {
	int index;		/* the number in the table: disk0sN */
	uint32_t start, sectors;
	uint8_t type;		/* MBR type, 0 for a GPT entry */
	uint8_t active;
	uint8_t bsdtype;	/* 0xa6 or the GPT type of OpenBSD's data */
};
struct disk {
	uint8_t bios;
	int ok;
	uint32_t sectors;
	int nparts;
	const char *table;	/* "MBR", "GPT" or NULL */
	struct part parts[MAXPARTS];
};
extern struct disk disks[];
extern int ndisks;
void disk_probe(void);
int disk_read(int disk, uint32_t lba, unsigned n, void *buf);

/* ffs.c: the file system of the root */
struct ffsfile {
	uint32_t ino;
	uint32_t mode;
	uint32_t size;
	uint8_t di[256];
};
int ffs_mount(int disk, uint32_t start, uint32_t sectors);
int ffs_lookup(const char *path, struct ffsfile *f);
int ffs_read(struct ffsfile *f, void *dst, uint32_t off, uint32_t len);
int ffs_next(struct ffsfile *dir, uint32_t *pos, char *name, int *isdir);

/* load.c, cmd.c */
struct spec {
	int disk, part;		/* part 0: the whole disk */
	char path[128];
};
int spec_parse(const char *s, struct spec *sp, const struct spec *def);
void spec_name(const struct spec *sp, char *buf, size_t n);
int spec_mount(const struct spec *sp);
int boot_kernel(const struct spec *sp, const char *args);
void cmd_loop(void);

extern struct spec defspec;

#endif
