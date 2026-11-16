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

/* acpimadt.c - the processors and the local APIC address, from the ACPI MADT. */
#include <sys/param.h>
#include <sys/systm.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>

uint32_t cpu_apicid[MAXCPUS]; /* the APIC ids the firmware lists as enabled */
int ncpufound;
uint32_t bsp_apicid;
uint64_t lapic_pa;
bool acpimadt_pcat; /* the 8259 PICs are there too */

#define GIB (1UL << 30)
#define SDT_HDR 36
#define SDT_MAX (1UL << 20)

static uint32_t rd32(const uint8_t *p) {
  uint32_t v;
  memcpy(&v, p, sizeof v);
  return v;
}

static uint64_t rd64(const uint8_t *p) {
  uint64_t v;
  memcpy(&v, p, sizeof v);
  return v;
}

static bool sum_ok(const uint8_t *p, size_t n) {
  uint8_t s = 0;
  while (n--)
    s += *p++;
  return s == 0;
}

/* a table in memory: the direct map has all RAM, the rest is mapped */
static const uint8_t *acpi_map(uint64_t pa, size_t len) {
  uint64_t top = (max_pfn * PAGE_SIZE + GIB - 1) & ~(GIB - 1);
  if (top < GIB)
    top = GIB; /* what boot.s mapped */
  if (pa + len > pa && pa + len <= top)
    return P2V(pa);
  return ioremap_wb(pa, len);
}

static const uint8_t *map_sdt(uint64_t pa, const char *sig) {
  const uint8_t *h = acpi_map(pa, SDT_HDR);
  if (!h || (sig && memcmp(h, sig, 4)))
    return NULL;
  uint32_t len = rd32(h + 4);
  if (len < SDT_HDR || len > SDT_MAX)
    return NULL;
  h = acpi_map(pa, len);
  return h && sum_ok(h, len) ? h : NULL;
}

static const uint8_t *scan_rsdp(uint64_t pa, size_t len) {
  const uint8_t *p = P2V(pa);
  for (size_t off = 0; off + 20 <= len; off += 16) {
    const uint8_t *r = p + off;
    if (memcmp(r, "RSD PTR ", 8) || !sum_ok(r, 20))
      continue;
    if (r[15] >= 2) {
      uint32_t l = rd32(r + 20);
      if (l < 36 || l > 4096 || !sum_ok(r, l))
        continue;
    }
    return r;
  }
  return NULL;
}

/* the RSDP is in the first KiB of the BIOS data area or in 0xe0000-0xfffff */
static const uint8_t *find_rsdp(void) {
  uint64_t ebda = (uint64_t)*(const uint16_t *)P2V(0x40e) << 4;
  const uint8_t *r = NULL;
  if (ebda >= 0x400 && ebda + 1024 <= 0xa0000)
    r = scan_rsdp(ebda, 1024);
  return r ? r : scan_rsdp(0xe0000, 0x20000);
}

static void add_cpu(uint32_t apicid) {
  if (apicid == 0xff || ncpufound == MAXCPUS)
    return;
  for (int i = 0; i < ncpufound; i++)
    if (cpu_apicid[i] == apicid)
      return;
  cpu_apicid[ncpufound++] = apicid;
}

/* Find the MADT ("APIC") and list the processors the firmware says are
   enabled; lapic_pa stays 0 when there is no ACPI. */
void acpimadt_init(void) {
  uint32_t r[4];
  cpuid4(1, 0, r);
  bsp_apicid = r[1] >> 24;

  const uint8_t *rsdp = find_rsdp();
  if (!rsdp)
    return;
  const uint8_t *sdt = NULL;
  size_t esz = 4;
  if (rsdp[15] >= 2 && rd64(rsdp + 24)) {
    sdt = map_sdt(rd64(rsdp + 24), "XSDT");
    esz = 8;
  }
  if (!sdt) {
    sdt = map_sdt(rd32(rsdp + 16), "RSDT");
    esz = 4;
  }
  if (!sdt)
    return;
  const uint8_t *madt = NULL;
  size_t n = (rd32(sdt + 4) - SDT_HDR) / esz;
  for (size_t i = 0; i < n && !madt; i++) {
    const uint8_t *e = sdt + SDT_HDR + i * esz;
    madt = map_sdt(esz == 8 ? rd64(e) : rd32(e), "APIC");
  }
  if (!madt)
    return;

  uint32_t len = rd32(madt + 4);
  if (len < 44)
    return;
  lapic_pa = rd32(madt + 36);
  acpimadt_pcat = rd32(madt + 40) & 1;
  for (uint32_t off = 44; off + 2 <= len;) {
    uint32_t l = madt[off + 1];
    if (l < 2 || off + l > len)
      break;
    switch (madt[off]) {
    case 0: /* processor local APIC: uid, id, flags (bit 0: enabled) */
      if (l >= 8 && (rd32(madt + off + 4) & 1))
        add_cpu(madt[off + 3]);
      break;
    case 5: /* local APIC address override */
      if (l >= 12 && rd64(madt + off + 4))
        lapic_pa = rd64(madt + off + 4);
      break;
    }
    off += l;
  }
}
