/* pci.c - PCI configuration space (mechanism #1: ports 0xcf8/0xcfc) and
   finding devices. */
#include "kernel.h"
#include "arch.h"
#include "dev.h"

static uint32_t addr_of(const struct pci_dev *d, int off) {
  return 0x80000000U | (uint32_t)d->bus << 16 | (uint32_t)d->dev << 11 | (uint32_t)d->fn << 8 | ((uint32_t)off & 0xfc);
}

uint32_t pci_read(const struct pci_dev *d, int off) {
  outl(0xcf8, addr_of(d, off));
  return inl(0xcfc);
}

void pci_write(const struct pci_dev *d, int off, uint32_t v) {
  outl(0xcf8, addr_of(d, off));
  outl(0xcfc, v);
}

/* the first device whose vendor:device is one of ids (pairs, 0-ended) */
bool pci_find(const uint16_t *ids, struct pci_dev *out) {
  for (int bus = 0; bus < 256; bus++)
    for (int dev = 0; dev < 32; dev++)
      for (int fn = 0; fn < 8; fn++) {
        struct pci_dev d = {(uint8_t)bus, (uint8_t)dev, (uint8_t)fn, 0, 0, 0, {0}};
        uint32_t id = pci_read(&d, 0);
        if ((id & 0xffff) == 0xffff) {
          if (fn == 0)
            break; /* no device: no functions */
          continue;
        }
        for (const uint16_t *p = ids; p[0]; p += 2)
          if ((id & 0xffff) == p[0] && (id >> 16) == p[1]) {
            d.vendor = (uint16_t)id;
            d.device = (uint16_t)(id >> 16);
            d.irq = (uint8_t)pci_read(&d, 0x3c);
            for (int b = 0; b < 6; b++)
              d.bar[b] = pci_read(&d, 0x10 + 4 * b);
            *out = d;
            return true;
          }
        if (fn == 0 && !(pci_read(&d, 0x0c) & 0x800000))
          break; /* not multi-function */
      }
  return false;
}
