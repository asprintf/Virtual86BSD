/* pci.c - the PCI bus: configuration space (mechanism #1: ports
   0xcf8/0xcfc), and the devices on it offered to the drivers.  Every bus
   from 0 to 255 is walked (the QEMU machines have one, and the devices of a
   bridge are found too, though nothing configures the bridge). */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/pci/pcivar.h>
#include <machine/cpu.h>
#include <machine/mainbus.h>
#include <locators.h>

#define PCIDEVS_DATA
#include <dev/pci/pcidevs.h>

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

void pci_devinfo(const struct pci_dev *d, char *buf, size_t n) {
  const char *vn = NULL, *pn = NULL;
  for (const struct pci_vendor_name *v = pci_vendors; v->name; v++)
    if (v->vendor == d->vendor)
      vn = v->name;
  for (const struct pci_product_name *p = pci_products; p->name; p++)
    if (p->vendor == d->vendor && p->product == d->device)
      pn = p->name;
  if (vn && pn)
    snprintf(buf, n, "\"%s %s\" rev 0x%02x", vn, pn, PCI_REVISION(d->class));
  else if (vn)
    snprintf(buf, n, "vendor \"%s\", unknown product 0x%04x rev 0x%02x", vn, d->device, PCI_REVISION(d->class));
  else
    snprintf(buf, n, "vendor 0x%04x, unknown product 0x%04x rev 0x%02x", d->vendor, d->device, PCI_REVISION(d->class));
}

/* the bus's own print function: where a device is (and what, as the line
   goes on, or at the start of the line of one that nothing took) */
static int pci_print(void *aux, const char *pnp) {
  struct pci_dev *d = aux;
  char info[96];
  if (pnp) {
    pci_devinfo(d, info, sizeof(info));
    printf("%s at %s", info, pnp);
  }
  if (d->bus)
    printf(" bus %d", d->bus);
  printf(" dev %d function %d", d->dev, d->fn);
  if (!pnp) {
    pci_devinfo(d, info, sizeof(info));
    printf(" %s", info);
  }
  return UNCONF;
}

/* a line of the table may say where (dev, function) */
static int pci_submatch(struct device *parent, void *match, void *aux) {
  struct cfdata *cf = match;
  struct pci_dev *d = aux;
  if (cf->cf_loc[PCICF_DEV] != -1 && cf->cf_loc[PCICF_DEV] != d->dev)
    return 0;
  if (cf->cf_loc[PCICF_FUNCTION] != -1 && cf->cf_loc[PCICF_FUNCTION] != d->fn)
    return 0;
  return config_match(parent, cf, aux);
}

static int pci_match(struct device *parent, void *match, void *aux) {
  struct mainbus_attach_args *ma = aux;
  (void)parent;
  (void)match;
  return !strcmp(ma->maa_name, "pci");
}

static void pci_attach(struct device *parent, struct device *self, void *aux) {
  (void)parent;
  (void)aux;
  printf("\n");
  for (int bus = 0; bus < 256; bus++)
    for (int dev = 0; dev < 32; dev++)
      for (int fn = 0; fn < 8; fn++) {
        struct pci_dev d = {(uint8_t)bus, (uint8_t)dev, (uint8_t)fn, 0, 0, 0, {0}, 0};
        uint32_t id = pci_read(&d, 0);
        if ((id & 0xffff) == 0xffff) {
          if (fn == 0)
            break; /* no device: no functions */
          continue;
        }
        d.vendor = (uint16_t)id;
        d.device = (uint16_t)(id >> 16);
        d.irq = (uint8_t)pci_read(&d, 0x3c);
        d.class = pci_read(&d, 0x08);
        for (int b = 0; b < 6; b++)
          d.bar[b] = pci_read(&d, 0x10 + 4 * b);
        config_found_sm(self, &d, pci_print, pci_submatch);
        if (fn == 0 && !(pci_read(&d, 0x0c) & 0x800000))
          break; /* not multi-function */
      }
}

struct cfdriver pci_cd = {NULL, "pci", DV_DULL, 0};
struct cfattach pci_ca = {sizeof(struct device), pci_match, pci_attach};
