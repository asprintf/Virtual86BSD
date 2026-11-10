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
/* "Intel 82540EM" rev 0x03 (into buf) */
void pci_devinfo(const struct pci_dev *d, char *buf, size_t n);

#endif
