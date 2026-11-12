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
   (vioblk, vio) attaches at virtio.  The virtqueue helpers below are for
   the drivers (vioblk keeps its own single-request code). */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pv/virtiovar.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>

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

/* ---- virtqueues (the legacy layout: the used ring starts on a page) ---- */

/* queue number index of the device at io: its memory, and the device told
   where it is (the queue's size is the device's) */
int virtio_queue_setup(uint16_t io, unsigned index, struct virtqueue *vq) {
  outw((uint16_t)(io + VIRTIO_R_QUEUE_SELECT), (uint16_t)index);
  unsigned size = inw((uint16_t)(io + VIRTIO_R_QUEUE_SIZE));
  if (!size)
    return -ENODEV; /* no such queue */
  size_t part1 = 16 * (size_t)size + 6 + 2 * (size_t)size;
  size_t total = PAGE_UP(part1) + PAGE_UP(6 + 8 * (size_t)size);
  int order = 0;
  while ((PAGE_SIZE << order) < total)
    order++;
  uint64_t q = alloc_pages(order);
  if (!q)
    return -ENOMEM;
  memset(P2V(q), 0, PAGE_SIZE << order);
  vq->io = io;
  vq->index = (uint16_t)index;
  vq->size = size;
  vq->desc = (volatile struct vring_desc *)P2V(q);
  vq->avail = (volatile uint16_t *)((uint8_t *)P2V(q) + 16 * (size_t)size);
  vq->used = (volatile uint16_t *)((uint8_t *)P2V(q) + PAGE_UP(part1));
  vq->avail_idx = vq->used_seen = 0;
  outl((uint16_t)(io + VIRTIO_R_QUEUE_PFN), (uint32_t)(q >> 12));
  return 0;
}

void virtio_queue_push(struct virtqueue *vq, unsigned head) {
  vq->avail[2 + vq->avail_idx % vq->size] = (uint16_t)head;
  mb(); /* the entry before the index */
  vq->avail_idx++;
  vq->avail[1] = vq->avail_idx;
}

void virtio_queue_notify(struct virtqueue *vq) {
  mb();
  outw((uint16_t)(vq->io + VIRTIO_R_QUEUE_NOTIFY), vq->index);
}

bool virtio_queue_pop(struct virtqueue *vq, unsigned *head, uint32_t *len) {
  if (vq->used[1] == vq->used_seen)
    return false;
  mb(); /* the index before the entry */
  volatile uint32_t *e = (volatile uint32_t *)(vq->used + 2) + 2 * (vq->used_seen % vq->size);
  *head = e[0];
  *len = e[1];
  vq->used_seen++;
  return true;
}

struct cfdriver virtio_cd = {NULL, "virtio", DV_DULL, 0};
struct cfattach virtio_ca = {sizeof(struct device), virtio_match, virtio_attach};
