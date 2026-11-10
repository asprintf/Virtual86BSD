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

/* disk.h - disks as devices (kern/subr_disk.c): what a controller driver
   (pciide, vioblk) offers as the aux of a device at it. */
#ifndef _SYS_DISK_H_
#define _SYS_DISK_H_

#include <sys/conf.h>

/* disk0, disk1 ...: the driver made the block device bd (not registered
   yet) for what it found; the disk registers it and says what it is.  A
   controller that tells more of where the disk is (pciide: channel and
   drive) puts this first in its own aux and has its own print function */
struct disk_attach_args {
  struct blockdev *da_bd;
  const char *da_desc; /* the drive's name: <QEMU HARDDISK> */
};

/* a partition table entry (kern/subr_disk.c): its number in the table, where
   it starts and how long it is, in sectors */
struct ptab_entry {
  int index; /* 1 ...: the name's number, disk0s<index> */
  uint64_t start, sectors;
  int type;  /* MBR: the type byte */
};

/* what a disk's table says (pure functions of the sectors, for the tests too) */
uint32_t crc32(const void *buf, size_t n);
int ptab_mbr(const uint8_t *s0, uint64_t disksectors, struct ptab_entry *e, bool *protective);
int ptab_gpt(const uint8_t *hdr, uint64_t disksectors, int (*get)(void *arg, uint64_t lba, unsigned n, void *buf),
             void *arg, struct ptab_entry *e, int maxe, const char **why);

/* the raw devices rdisk0, rdisk0s1 ...: a character major of their own */
#define RDISK_MAJOR 241
extern bool rdisk_present;

#endif
