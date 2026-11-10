/* pcivar.h - PCI devices (dev/pci/pci.c). */
#ifndef _DEV_PCI_PCIVAR_H_
#define _DEV_PCI_PCIVAR_H_

#include <sys/systm.h>

/* PCI (dev/pci/pci.c) */
struct pci_dev {
  uint8_t bus, dev, fn, irq;
  uint16_t vendor, device;
  uint32_t bar[6];
};
uint32_t pci_read(const struct pci_dev *d, int off);
void pci_write(const struct pci_dev *d, int off, uint32_t v);
bool pci_find(const uint16_t *ids, struct pci_dev *out);

#endif
