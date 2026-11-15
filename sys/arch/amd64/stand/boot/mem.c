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

/* mem.c - the memory map of the BIOS (INT 15h, E820) */
#include "boot.h"

struct mrange memmap[64];
int nmem;
unsigned basemem;

static uint8_t e820buf[24] LOW;

void mem_probe(void) {
	struct biosregs r = {0};
	bioscall(0x12, &r);
	basemem = r.eax & 0xffff;
	uint32_t next = 0;
	do {
		memset(&r, 0, sizeof(r));
		memset(e820buf, 0, sizeof(e820buf));
		r.eax = 0xe820;
		r.ebx = next;
		r.ecx = 24;
		r.edx = 0x534d4150;
		r.edi = (uint32_t)e820buf;
		bioscall(0x15, &r);
		if ((r.flags & CF) || r.eax != 0x534d4150)
			break;
		next = r.ebx;
		if (nmem < 64) {
			memcpy(&memmap[nmem].base, e820buf, 8);
			memcpy(&memmap[nmem].len, e820buf + 8, 8);
			memcpy(&memmap[nmem].type, e820buf + 16, 4);
			if (memmap[nmem].len)
				nmem++;
		}
	} while (next);
}

/* whether [lo, hi) is in one range of usable memory */
int mem_usable(uint32_t lo, uint32_t hi) {
	for (int i = 0; i < nmem; i++)
		if (memmap[i].type == 1 && memmap[i].base <= lo && (uint64_t)hi <= memmap[i].base + memmap[i].len)
			return 1;
	return 0;
}

uint32_t mem_above1m(void) {
	uint64_t t = 0;
	for (int i = 0; i < nmem; i++)
		if (memmap[i].type == 1 && memmap[i].base >= 0x100000)
			t += memmap[i].len;
	return (uint32_t)(t >> 20);
}
