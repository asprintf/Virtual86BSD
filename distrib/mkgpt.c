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

/* mkgpt.c - write a GUID partition table into a disk image: the protective
   MBR, the primary table (header and entries) after it and the backup table
   at the end of the disk.  One partition, of OpenBSD's data type (what
   a V86BSD file system has; MBR type 0xa6), at the given start and size.
   The GUIDs come from the layout, so the same layout gives the same image.
   The boot code in the MBR is left to the loader's installation.  Written
   in POSIX C for the host (mkdisk-ffs.sh builds it).

   Usage: mkgpt -p start:sectors image   (the image is a file of the size of the disk) */
#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#include <sys/stat.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NENT 128
#define ESIZE 128
#define ESECT (NENT * ESIZE / 512)	/* sectors of the entries */

static const uint8_t bsdtype[16] = {0xa0, 0xc7, 0x4c, 0x82, 0xa8, 0x36, 0xe3, 0x11,
				    0x89, 0x0a, 0x95, 0x25, 0x19, 0xad, 0x3f, 0x61};

static uint32_t crc32(const uint8_t *p, size_t n) {
  uint32_t c = 0xffffffffU;
  while (n--) {
    c ^= *p++;
    for (int k = 0; k < 8; k++)
      c = c & 1 ? 0xedb88320U ^ (c >> 1) : c >> 1;
  }
  return ~c;
}

static void put32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; i++)
    p[i] = (uint8_t)(v >> 8 * i);
}

static void put64(uint8_t *p, uint64_t v) {
  put32(p, (uint32_t)v);
  put32(p + 4, (uint32_t)(v >> 32));
}

/* a version 4 GUID from the layout (xorshift of a seed) */
static void guid(uint8_t *g, uint64_t seed) {
  uint64_t x = seed | 1;
  for (int i = 0; i < 16; i++) {
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    g[i] = (uint8_t)(x >> 24);
  }
  g[7] = (uint8_t)((g[7] & 0x0f) | 0x40);
  g[8] = (uint8_t)((g[8] & 0x3f) | 0x80);
}

static void header(uint8_t *h, uint64_t me, uint64_t other, uint64_t total, uint64_t entlba, const uint8_t *dguid,
                   uint32_t ecrc) {
  memset(h, 0, 512);
  memcpy(h, "EFI PART", 8);
  put32(h + 8, 0x00010000);
  put32(h + 12, 92);
  put64(h + 24, me);
  put64(h + 32, other);
  put64(h + 40, 34);
  put64(h + 48, total - 34);
  memcpy(h + 56, dguid, 16);
  put64(h + 72, entlba);
  put32(h + 80, NENT);
  put32(h + 84, ESIZE);
  put32(h + 88, ecrc);
  put32(h + 16, crc32(h, 92));
}

static int write_at(FILE *f, uint64_t lba, const void *buf, size_t n) {
  if (fseeko(f, (off_t)(lba * 512), SEEK_SET) || fwrite(buf, 1, n, f) != n) {
    perror("mkgpt: write");
    return -1;
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc != 4 || strcmp(argv[1], "-p")) {
    fprintf(stderr, "usage: mkgpt -p start:sectors image\n");
    return 2;
  }
  unsigned long long start, len;
  if (sscanf(argv[2], "%llu:%llu", &start, &len) != 2) {
    fprintf(stderr, "mkgpt: -p start:sectors\n");
    return 2;
  }
  struct stat st;
  if (stat(argv[3], &st) || !S_ISREG(st.st_mode)) {
    fprintf(stderr, "mkgpt: %s: not a file\n", argv[3]);
    return 1;
  }
  uint64_t total = (uint64_t)st.st_size / 512;
  if (total < 2048 || start < 34 || len == 0 || start + len > total - 33) {
    fprintf(stderr, "mkgpt: the partition does not fit in the disk of %llu sectors\n", (unsigned long long)total);
    return 1;
  }
  static uint8_t ents[ESECT * 512], pri[512], bak[512], mbr[512];
  uint8_t dguid[16], pguid[16];
  guid(dguid, total * 0x9e3779b97f4a7c15ULL ^ start);
  guid(pguid, (start + len) * 0xbf58476d1ce4e5b9ULL ^ total);
  memcpy(ents, bsdtype, 16);
  memcpy(ents + 16, pguid, 16);
  put64(ents + 32, start);
  put64(ents + 40, start + len - 1);
  const char *name = "v86bsd";
  for (int i = 0; name[i]; i++)
    ents[56 + 2 * i] = (uint8_t)name[i];
  uint32_t ecrc = crc32(ents, sizeof(ents));
  header(pri, 1, total - 1, total, 2, dguid, ecrc);
  header(bak, total - 1, 1, total, total - 33, dguid, ecrc);

  memset(mbr, 0, 512);
  mbr[446 + 2] = 2;	/* the protective entry: CHS 0/0/2 to the end, type 0xee, from LBA 1 */
  mbr[446 + 4] = 0xee;
  mbr[446 + 5] = mbr[446 + 6] = mbr[446 + 7] = 0xff;
  put32(mbr + 446 + 8, 1);
  put32(mbr + 446 + 12, total - 1 > 0xffffffffULL ? 0xffffffffU : (uint32_t)(total - 1));
  mbr[510] = 0x55;
  mbr[511] = 0xaa;

  FILE *f = fopen(argv[3], "r+b");
  if (!f) {
    perror(argv[3]);
    return 1;
  }
  int bad = write_at(f, 0, mbr, 512) || write_at(f, 1, pri, 512) || write_at(f, 2, ents, sizeof(ents)) ||
            write_at(f, total - 33, ents, sizeof(ents)) || write_at(f, total - 1, bak, 512);
  if (fclose(f) || bad)
    return 1;
  return 0;
}
