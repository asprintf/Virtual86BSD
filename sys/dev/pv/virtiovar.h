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

/* virtiovar.h - virtio devices (dev/pv/virtio.c): a virtio device found on
   PCI, and what the driver for its kind is offered. */
#ifndef _DEV_PV_VIRTIOVAR_H_
#define _DEV_PV_VIRTIOVAR_H_

#include <dev/pci/pcivar.h>

/* the kinds of device (the PCI subsystem ID of a legacy device) */
#define VIRTIO_ID_NETWORK 1
#define VIRTIO_ID_BLOCK 2
#define VIRTIO_ID_CONSOLE 3
#define VIRTIO_ID_ENTROPY 4
#define VIRTIO_ID_BALLOON 5
#define VIRTIO_ID_SCSI 8

/* the legacy ("transitional") interface: registers in I/O BAR 0 (virtio
   1.2, 4.1.4.8 and 2.7) */
#define VIRTIO_R_DEV_FEATURES 0x00
#define VIRTIO_R_GUEST_FEATURES 0x04
#define VIRTIO_R_QUEUE_PFN 0x08
#define VIRTIO_R_QUEUE_SIZE 0x0c
#define VIRTIO_R_QUEUE_SELECT 0x0e
#define VIRTIO_R_QUEUE_NOTIFY 0x10
#define VIRTIO_R_STATUS 0x12
#define VIRTIO_R_ISR 0x13 /* reading it acknowledges the interrupt: bit 0 a queue, bit 1 the configuration */
#define VIRTIO_R_CONFIG 0x14 /* the device's configuration (without MSI-X) */

#define VIRTIO_S_ACK 1
#define VIRTIO_S_DRIVER 2
#define VIRTIO_S_DRIVER_OK 4
#define VIRTIO_S_FAILED 128

#define VRING_DESC_F_NEXT 1
#define VRING_DESC_F_WRITE 2 /* the device writes this buffer */
#define VRING_AVAIL_F_NO_INTERRUPT 1

struct vring_desc {
  uint64_t addr;
  uint32_t len;
  uint16_t flags, next;
};

/* a split virtqueue: descriptors, the available ring (flags, idx, ring[];
   the driver's), the used ring (flags, idx, {id, len}[]; the device's) */
struct virtqueue {
  uint16_t io, index;
  unsigned size;
  volatile struct vring_desc *desc;
  volatile uint16_t *avail, *used;
  uint16_t avail_idx, used_seen;
};

int virtio_queue_setup(uint16_t io, unsigned index, struct virtqueue *vq); /* 0, or -errno */
void virtio_queue_push(struct virtqueue *vq, unsigned head);               /* a chain, by its first descriptor */
void virtio_queue_notify(struct virtqueue *vq);
bool virtio_queue_pop(struct virtqueue *vq, unsigned *head, uint32_t *len); /* a chain the device is done with */

/* the aux of a device at virtio */
struct virtio_attach_args {
  struct pci_dev *va_pd; /* the PCI device (its BAR 0, the I/O ports) */
  int va_devid;          /* VIRTIO_ID_ */
};

#endif
