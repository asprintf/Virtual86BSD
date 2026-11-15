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

/* disk.c - the disks of the BIOS (disk0 is drive 0x80, as the kernel names
   them) and their partitions: the four of an MBR, or the entries of a GPT
   when the MBR is a protective one.  The partitions are numbered as the
   kernel numbers them (disk0s1: the first slot of the table), and a GPT
   must have good CRC-32s, as for the kernel. */
#include "boot.h"

struct disk disks[MAXDISKS];
int ndisks;

static uint8_t dap[16] LOW;
static uint8_t params[32] LOW;
static uint8_t gptbuf[16384];

#define BOUNCE 0x60000	/* the BIOS's buffer: 64 KiB below 1 MiB */

int disk_read(int d, uint32_t lba, unsigned n, void *buf) {
	if (d < 0 || d >= ndisks || !disks[d].ok)
		return -1;
	uint8_t *p = buf;
	while (n) {
		unsigned c = n > 64 ? 64 : n;
		int rc = -1;
		for (int tries = 0; tries < 3 && rc; tries++) {
			memset(dap, 0, sizeof(dap));
			dap[0] = 16;
			dap[2] = (uint8_t)c;
			dap[7] = BOUNCE >> 12;	/* the segment */
			memcpy(dap + 8, &lba, 4);
			struct biosregs r = {.eax = 0x4200, .edx = disks[d].bios, .esi = (uint32_t)dap};
			bioscall(0x13, &r);
			if (!(r.flags & CF)) {
				rc = 0;
				break;
			}
			struct biosregs z = {.eax = 0, .edx = disks[d].bios};
			bioscall(0x13, &z);
		}
		if (rc)
			return -1;
		memcpy(p, (void *)BOUNCE, c * 512);
		p += c * 512;
		lba += c;
		n -= c;
	}
	return 0;
}

static uint32_t le32(const uint8_t *p) {
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* OpenBSD's GPT type for data, which a V86BSD file system has (as MBR type 0xa6) */
static const uint8_t bsdguid[16] = {0xa0, 0xc7, 0x4c, 0x82, 0xa8, 0x36, 0xe3, 0x11,
				    0x89, 0x0a, 0x95, 0x25, 0x19, 0xad, 0x3f, 0x61};

static void addpart(struct disk *dk, int index, uint32_t start, uint32_t len, uint8_t type, uint8_t active, uint8_t bsd) {
	if (dk->nparts >= MAXPARTS)
		return;
	struct part *p = &dk->parts[dk->nparts++];
	p->index = index;
	p->start = start;
	p->sectors = len;
	p->type = type;
	p->active = active;
	p->bsdtype = bsd;
}

static void gpt(int d, struct disk *dk, const uint8_t *hdr) {
	dk->table = NULL;
	if (memcmp(hdr, "EFI PART", 8))
		return;
	uint32_t hsize = le32(hdr + 12);
	uint8_t h[512];
	if (hsize < 92 || hsize > 512)
		return;
	memcpy(h, hdr, hsize);
	uint32_t crc = le32(h + 16);
	memset(h + 16, 0, 4);
	if (crc32(h, hsize) != crc)
		return;
	uint32_t lba = le32(hdr + 72), lbahi = le32(hdr + 76);
	uint32_t nent = le32(hdr + 80), esize = le32(hdr + 84), ecrc = le32(hdr + 88);
	if (lbahi || esize < 128 || esize > 512 || (esize & (esize - 1)) || nent == 0 || nent > 128 ||
	    nent * esize > sizeof(gptbuf))
		return;
	unsigned nsect = (nent * esize + 511) / 512;
	if (lba + nsect > dk->sectors || disk_read(d, lba, nsect, gptbuf))
		return;
	if (crc32(gptbuf, nent * esize) != ecrc)
		return;
	dk->table = "GPT";
	uint32_t first = le32(hdr + 40), last = le32(hdr + 48);
	for (uint32_t i = 0; i < nent; i++) {
		const uint8_t *p = gptbuf + i * esize;
		int used = 0;
		for (int k = 0; k < 16; k++)
			used |= p[k] != 0;
		if (!used || le32(p + 36) || le32(p + 44) || le32(p + 32) < first || le32(p + 40) > last ||
		    le32(p + 40) < le32(p + 32) || le32(p + 40) >= dk->sectors)
			continue;
		addpart(dk, (int)i + 1, le32(p + 32), le32(p + 40) - le32(p + 32) + 1, 0, 0, !memcmp(p, bsdguid, 16) ? 0xa6 : 0);
	}
}

static void scan(int d, struct disk *dk) {
	uint8_t s[1024];
	dk->nparts = 0;
	dk->table = NULL;
	if (disk_read(d, 0, 1, s) || s[510] != 0x55 || s[511] != 0xaa)
		return;
	int prot = 0;
	for (int i = 0; i < 4; i++) {
		const uint8_t *p = s + 446 + 16 * i;
		uint32_t start = le32(p + 8), len = le32(p + 12);
		if (!p[4] || !len)
			continue;
		if (p[4] == 0xee) {
			prot = 1;
			continue;
		}
		if ((uint64_t)start + len > dk->sectors)
			continue;
		dk->table = "MBR";
		addpart(dk, i + 1, start, len, p[4], p[0] == 0x80, p[4] == 0xa6 ? 0xa6 : 0);
	}
	if (prot) {
		dk->nparts = 0;
		if (!disk_read(d, 1, 1, s))
			gpt(d, dk, s);
	}
}

void disk_probe(void) {
	for (int i = 0; i < MAXDISKS; i++) {
		struct disk *dk = &disks[i];
		dk->bios = (uint8_t)(0x80 + i);
		dk->ok = 0;
		struct biosregs r = {.eax = 0x4100, .ebx = 0x55aa, .edx = dk->bios};
		bioscall(0x13, &r);
		if ((r.flags & CF) || (r.ebx & 0xffff) != 0xaa55)
			continue;
		memset(params, 0, sizeof(params));
		params[0] = 0x1e;
		struct biosregs q = {.eax = 0x4800, .edx = dk->bios, .esi = (uint32_t)params};
		bioscall(0x13, &q);
		if (q.flags & CF)
			continue;
		uint32_t lo = le32(params + 16), hi = le32(params + 20);
		dk->sectors = hi ? 0xffffffffU : lo;
		dk->ok = 1;
		ndisks = i + 1;
		scan(i, dk);
	}
}
