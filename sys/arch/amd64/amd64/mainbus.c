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

/* mainbus.c - the root of the device tree: the processor and the PCI bus
   (the ISA bus is the PCI-ISA bridge's). */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <machine/mainbus.h>

static int mainbus_print(void *aux, const char *pnp) {
  struct mainbus_attach_args *ma = aux;
  if (pnp)
    printf("%s at %s", ma->maa_name, pnp);
  if (!strcmp(ma->maa_name, "pci"))
    printf(" bus %d", ma->maa_bus);
  return UNCONF;
}

static int mainbus_match(struct device *parent, void *match, void *aux) {
  (void)parent;
  (void)match;
  (void)aux;
  return 1;
}

static void mainbus_attach(struct device *parent, struct device *self, void *aux) {
  (void)parent;
  (void)aux;
  struct mainbus_attach_args ma = {"cpu", 0};
  printf("\n");
  config_found(self, &ma, mainbus_print);
  ma.maa_name = "pci";
  config_found(self, &ma, mainbus_print);
}

struct cfdriver mainbus_cd = {NULL, "mainbus", DV_DULL, 0};
struct cfattach mainbus_ca = {sizeof(struct device), mainbus_match, mainbus_attach};
