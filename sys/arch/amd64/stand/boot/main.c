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

/* main.c - stage 2: find what the BIOS has, say so, and ask what to boot */
#include "boot.h"

static int a20_on(void) {
	volatile uint32_t *lo = (volatile uint32_t *)phys(0x500), *hi = (volatile uint32_t *)phys(0x100500);
	uint32_t s1 = *lo, s2 = *hi;
	*lo = 0x12345678;
	*hi = 0x87654321;
	int on = *lo == 0x12345678;
	*lo = s1;
	*hi = s2;
	return on;
}

/* the partition to boot from: the active one, else one of a BSD type, else
   any, that has a file system, on the disk the BIOS booted from, else on
   another */
static int find_default(int bootdisk) {
	for (int k = 0; k < ndisks; k++) {
		int d = k == 0 ? bootdisk : k <= bootdisk ? k - 1 : k;
		if (d < 0 || d >= ndisks || !disks[d].ok)
			continue;
		for (int pass = 0; pass < 3; pass++)
			for (int i = 0; i < disks[d].nparts; i++) {
				const struct part *p = &disks[d].parts[i];
				if (pass == 0 ? !p->active : pass == 1 ? !p->bsdtype : 0)
					continue;
				if (!ffs_mount(d, p->start, p->sectors)) {
					defspec.disk = d;
					defspec.part = p->index;
					return 0;
				}
			}
	}
	return -1;
}

void main(unsigned bootdrive) {
	cons_init();
	mem_probe();
	disk_probe();
	printf("\nprobing: pc0 com0 mem[%uK %uM a20=%s]\n", basemem, mem_above1m(), a20_on() ? "on" : "off");
	int bootdisk = (int)bootdrive - 0x80;
	printf("disk:");
	for (int i = 0; i < ndisks; i++)
		if (disks[i].ok)
			printf(" disk%d%s", i, i == bootdisk ? "*" : "");
	printf("\n>> V86BSD/amd64 BOOT " VERSION "\n");
	if (find_default(bootdisk))
		printf("no FFS2 file system found\n");
	cmd_loop();
}
