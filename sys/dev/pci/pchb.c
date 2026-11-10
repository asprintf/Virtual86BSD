/* pchb.c - the PCI host bridge (class bridge, subclass host): the bridge
   between the processor and the bus, pchb0.  There is nothing to set up:
   the bridge is there so the device tree shows it, as OpenBSD's does. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

static int pchb_match(struct device *parent, void *match, void *aux) {
  struct pci_dev *d = aux;
  (void)parent;
  (void)match;
  return PCI_CLASS(d->class) == PCI_CLASS_BRIDGE && PCI_SUBCLASS(d->class) == PCI_SUBCLASS_BRIDGE_HOST;
}

static void pchb_attach(struct device *parent, struct device *self, void *aux) {
  (void)parent;
  (void)self;
  (void)aux;
  printf("\n");
}

struct cfdriver pchb_cd = {NULL, "pchb", DV_DULL, 0};
struct cfattach pchb_ca = {sizeof(struct device), pchb_match, pchb_attach};
