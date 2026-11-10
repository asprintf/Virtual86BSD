/*
 * Copyright (c) 2026, Joost Orchidee Winter
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

/* pcivar.h - PCI devices (dev/pci/pci.c): what a driver is given when the
   bus offers it a device, and the configuration space. */
#ifndef _DEV_PCI_PCIVAR_H_
#define _DEV_PCI_PCIVAR_H_

#include <sys/systm.h>

/* PCI (dev/pci/pci.c): the aux of a device at pci (pci_attach_args) */
struct pci_dev {
  uint8_t bus, dev, fn, irq;
  uint16_t vendor, device;
  uint32_t bar[6];
  uint32_t class; /* register 8: class, subclass, interface, revision */
};
uint32_t pci_read(const struct pci_dev *d, int off);
void pci_write(const struct pci_dev *d, int off, uint32_t v);
/* "Intel 82540EM" rev 0x03, or vendor "Kioxia", unknown product 0x0010 rev 0x01 (into buf) */
void pci_devinfo(const struct pci_dev *d, char *buf, size_t n);

#endif
