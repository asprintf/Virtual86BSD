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

/* pcib.c - the PCI-ISA bridge (class bridge, subclass ISA), pcib0: it
   offers the ISA bus, isa0, its place in the tree.  The bus is attached
   when the PCI bus has been walked (config_defer), as OpenBSD's: the
   devices of the bus come after the PCI ones. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/isa/isavar.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

static int pcib_match(struct device *parent, void *match, void *aux) {
  struct pci_dev *d = aux;
  (void)parent;
  (void)match;
  return PCI_CLASS(d->class) == PCI_CLASS_BRIDGE && PCI_SUBCLASS(d->class) == PCI_SUBCLASS_BRIDGE_ISA;
}

static void pcib_callback(struct device *self) {
  struct isabus_attach_args iba = {"isa"};
  config_found(self, &iba, NULL);
}

static void pcib_attach(struct device *parent, struct device *self, void *aux) {
  (void)parent;
  (void)aux;
  printf("\n");
  config_defer(self, pcib_callback);
}

struct cfdriver pcib_cd = {NULL, "pcib", DV_DULL, 0};
struct cfattach pcib_ca = {sizeof(struct device), pcib_match, pcib_attach};
