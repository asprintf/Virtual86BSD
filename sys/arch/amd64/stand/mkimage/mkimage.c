/*
 * Copyright (c) 2026, Joost Orchidee Winter
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

/* mkimage.c - make a flat Multiboot image from the kernel ELF file.

   Usage: mkimage kernel.elf image

   The image holds the loadable segments from the lowest physical address
   (virtual - KERNEL_BASE) on; the multiboot header in its first 8 KiB gets
   the address-kludge fields, so a loader (QEMU -kernel, GRUB) needs no ELF
   support for a 64-bit file.  Written in POSIX C for the host. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define KERNEL_BASE 0xffffffff80000000ULL

static uint64_t rd(const unsigned char *p, int n) {
  uint64_t v = 0;
  for (int i = n - 1; i >= 0; i--)
    v = v << 8 | p[i];
  return v;
}

static void wr32(unsigned char *p, uint32_t v) {
  for (int i = 0; i < 4; i++)
    p[i] = (unsigned char)(v >> 8 * i);
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: mkimage kernel.elf image\n");
    return 2;
  }
  FILE *f = fopen(argv[1], "rb");
  if (!f) {
    perror(argv[1]);
    return 1;
  }
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  rewind(f);
  unsigned char *e = malloc((size_t)size);
  if (!e || fread(e, 1, (size_t)size, f) != (size_t)size) {
    fprintf(stderr, "mkimage: cannot read %s\n", argv[1]);
    return 1;
  }
  fclose(f);
  if (memcmp(e, "\177ELF", 4) || e[4] != 2) {
    fprintf(stderr, "mkimage: %s: not an ELF64 file\n", argv[1]);
    return 1;
  }
  uint64_t entry = rd(e + 24, 8), phoff = rd(e + 32, 8);
  unsigned phnum = (unsigned)rd(e + 56, 2);
  uint64_t lo = UINT64_MAX, fend = 0, mend = 0;
  for (unsigned i = 0; i < phnum; i++) {
    const unsigned char *ph = e + phoff + 56 * i;
    if (rd(ph, 4) != 1 || rd(ph + 40, 8) == 0)
      continue;
    uint64_t pa = rd(ph + 16, 8) - KERNEL_BASE;
    if (pa < lo)
      lo = pa;
    if (pa + rd(ph + 32, 8) > fend)
      fend = pa + rd(ph + 32, 8);
    if (pa + rd(ph + 40, 8) > mend)
      mend = pa + rd(ph + 40, 8);
  }
  if (lo == UINT64_MAX) {
    fprintf(stderr, "mkimage: no loadable segments\n");
    return 1;
  }
  size_t isz = (size_t)(fend - lo);
  unsigned char *img = calloc(1, isz);
  for (unsigned i = 0; i < phnum; i++) {
    const unsigned char *ph = e + phoff + 56 * i;
    if (rd(ph, 4) != 1 || rd(ph + 32, 8) == 0)
      continue;
    uint64_t pa = rd(ph + 16, 8) - KERNEL_BASE;
    memcpy(img + (pa - lo), e + rd(ph + 8, 8), (size_t)rd(ph + 32, 8));
  }
  size_t h;
  for (h = 0; h + 32 <= isz && h < 8192; h += 4)
    if (rd(img + h, 4) == 0x1BADB002)
      break;
  if (h + 32 > isz || h >= 8192) {
    fprintf(stderr, "mkimage: no multiboot header in the first 8 KiB\n");
    return 1;
  }
  wr32(img + h + 12, (uint32_t)(lo + h));
  wr32(img + h + 16, (uint32_t)lo);
  wr32(img + h + 20, (uint32_t)fend);
  wr32(img + h + 24, (uint32_t)mend);
  wr32(img + h + 28, (uint32_t)(entry - KERNEL_BASE));
  FILE *o = fopen(argv[2], "wb");
  if (!o || fwrite(img, 1, isz, o) != isz || fclose(o)) {
    perror(argv[2]);
    return 1;
  }
  return 0;
}
