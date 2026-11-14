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

/* machdep.c - amd64 start-up, from the multiboot hand-over to kmain: the
   consoles, the descriptor tables, the memory map and the boot arguments. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>

extern char _kernel_end[];
static uint64_t ranges[64], reserved[32];
static int nranges, nreserved;

static uint32_t rd32(uint64_t phys) { return *(uint32_t *)P2V(phys); }

static void parse_multiboot(uint32_t magic, uint64_t mbi) {
  if (magic != 0x2BADB002)
    panic("not started by a multiboot loader (magic %#x)", magic);
  uint32_t flags = rd32(mbi);
  if (flags & 4)
    bootargs_init(P2V((uint64_t)rd32(mbi + 16)));
  reserved[nreserved++] = PAGE_DOWN(mbi);
  reserved[nreserved++] = PAGE_UP(mbi + 120);
  if (flags & 8) {
    uint32_t count = rd32(mbi + 20), addr = rd32(mbi + 24);
    for (uint32_t i = 0; i < count && nreserved < 30; i++) {
      reserved[nreserved++] = PAGE_DOWN(rd32(addr + 16 * i));
      reserved[nreserved++] = PAGE_UP(rd32(addr + 16 * i + 4));
    }
  }
  if (flags & 64) {
    uint32_t len = rd32(mbi + 44), addr = rd32(mbi + 48);
    for (uint32_t off = 0; off < len && nranges < 32;) {
      uint32_t size = rd32(addr + off);
      uint64_t base = *(uint64_t *)P2V(addr + off + 4);
      uint64_t l = *(uint64_t *)P2V(addr + off + 12);
      uint32_t type = rd32(addr + off + 20);
      if (type == 1) {
        ranges[2 * nranges] = base;
        ranges[2 * nranges + 1] = base + l;
        nranges++;
      }
      off += size + 4;
    }
  } else if (flags & 1) {
    ranges[0] = 0x100000;
    ranges[1] = 0x100000 + (uint64_t)rd32(mbi + 8) * 1024;
    nranges = 1;
  }
  if (!nranges)
    panic("no memory map");
}

void init_x86_64(uint32_t magic, uint32_t mbi) {
  serial_init();
  vga_init();
  printf("%s", version);
  arch_init(); /* the tables first: a trap from here on prints its registers */
  parse_multiboot(magic, mbi);
  page_init((uint64_t)_kernel_end - KERNEL_BASE, ranges, nranges, reserved, nreserved);
  vm_init();
  page_init_high(ranges, nranges, reserved, nreserved);

  /* as OpenBSD: the memory there is, then what the kernel has left */
  uint64_t real = 0, avail = free_pages_count * PAGE_SIZE;
  for (int i = 0; i < nranges; i++)
    real += ranges[2 * i + 1] - ranges[2 * i];
  printf("real mem = %lu (%luMB)\n", real, (real + 0x80000) >> 20);
  printf("avail mem = %lu (%luMB)\n", avail, (avail + 0x80000) >> 20);
  /* what follows the name of the kernel (the loader puts that first) */
  const char *args = kernel_cmdline;
  while (*args && *args != ' ')
    args++;
  while (*args == ' ')
    args++;
  if (*args)
    printf("bootargs: %s\n", args);
}
