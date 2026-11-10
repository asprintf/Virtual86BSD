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

/* virtio.c - the virtio transport on PCI: the legacy ("transitional")
   interface, vendor 1af4 devices 1000..103f, registers in I/O BAR 0.  What
   kind of device it is, is in the subsystem ID; the driver for that kind
   (vioblk) attaches at virtio. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pv/virtiovar.h>

static int virtio_match(struct device *parent, void *match, void *aux) {
  struct pci_dev *d = aux;
  (void)parent;
  (void)match;
  return d->vendor == PCI_VENDOR_QUMRANET && d->device >= 0x1000 && d->device <= 0x103f && (d->bar[0] & 1);
}

static const char *virtio_devname(int id) {
  switch (id) {
  case VIRTIO_ID_NETWORK: return "network";
  case VIRTIO_ID_BLOCK: return "block";
  case VIRTIO_ID_CONSOLE: return "console";
  case VIRTIO_ID_ENTROPY: return "entropy";
  case VIRTIO_ID_BALLOON: return "memory balloon";
  case VIRTIO_ID_SCSI: return "SCSI";
  }
  return "unknown";
}

static int virtio_print(void *aux, const char *pnp) {
  struct virtio_attach_args *va = aux;
  if (pnp)
    printf("virtio %s (%d) at %s", virtio_devname(va->va_devid), va->va_devid, pnp);
  return UNCONF;
}

static void virtio_attach(struct device *parent, struct device *self, void *aux) {
  struct pci_dev *d = aux;
  struct virtio_attach_args va;
  (void)parent;
  printf("\n");
  va.va_pd = d;
  va.va_devid = (int)(pci_read(d, 0x2c) >> 16); /* subsystem ID */
  config_found(self, &va, virtio_print);
}

struct cfdriver virtio_cd = {NULL, "virtio", DV_DULL, 0};
struct cfattach virtio_ca = {sizeof(struct device), virtio_match, virtio_attach};
