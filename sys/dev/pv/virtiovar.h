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

/* the aux of a device at virtio */
struct virtio_attach_args {
  struct pci_dev *va_pd; /* the PCI device (its BAR 0, the I/O ports) */
  int va_devid;          /* VIRTIO_ID_ */
};

#endif
