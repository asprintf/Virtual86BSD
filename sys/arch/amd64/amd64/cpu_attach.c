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

/* cpu_attach.c - the processors as devices: cpu0 is the one the kernel
   started on, the others (cpu1 ...) are started here. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <machine/cpu.h>
#include <machine/mainbus.h>

static int cpu_match(struct device *parent, void *match, void *aux) {
  struct mainbus_attach_args *ma = aux;
  (void)parent;
  (void)match;
  return !strcmp(ma->maa_name, "cpu");
}

/* as OpenBSD: what the processor is, its clock and family-model-stepping in hex */
static void cpu_print_id(struct device *self, struct cpu_info *ci) {
  uint32_t eax = ci->ci_signature;
  unsigned family = (eax >> 8) & 15, model = (eax >> 4) & 15, step = eax & 15;
  if (family == 15)
    family += (eax >> 20) & 255;
  if (family >= 6)
    model += ((eax >> 16) & 15) << 4;
  const char *s = ci->ci_brand;
  while (*s == ' ')
    s++;
  uint64_t mhz100 = (tsc_frequency() + 5000) / 10000;
  printf("%s: %s, %lu.%02lu MHz, %02x-%02x-%02x\n", self->dv_xname, *s ? s : "unknown processor", mhz100 / 100,
         mhz100 % 100, family, model, step);
  if (ci->ci_apicid_seen != ci->ci_apicid)
    printf("%s: its apid is %u, not the %u the firmware lists\n", self->dv_xname, ci->ci_apicid_seen, ci->ci_apicid);
}

static void cpu_attach(struct device *parent, struct device *self, void *aux) {
  struct mainbus_attach_args *ma = aux;
  (void)parent;
  if (self->dv_unit == 0) {
    struct cpu_info *ci = &cpu_info_primary;
    printf(": apid %u (boot processor)\n", ma->maa_apicid);
    ci->ci_apicid = ma->maa_apicid;
    cpu_identify(ci);
    const char *s = ci->ci_brand;
    while (*s == ' ')
      s++;
    snprintf(cpu_model, sizeof cpu_model, "%s", *s ? s : "unknown processor"); /* hw.model */
    cpu_print_id(self, ci);
    lapic_init(self->dv_xname);
    cpu_boot(ci);
    return;
  }
  printf(": apid %u (application processor)\n", ma->maa_apicid);
  struct cpu_info *ci = cpu_alloc(self->dv_unit, ma->maa_apicid);
  if (!ci) {
    printf("%s: out of memory\n", self->dv_xname);
    return;
  }
  if (cpu_boot(ci) == 0)
    cpu_print_id(self, ci);
}

struct cfdriver cpu_cd = {NULL, "cpu", DV_CPU, 0};
struct cfattach cpu_ca = {sizeof(struct device), cpu_match, cpu_attach};
