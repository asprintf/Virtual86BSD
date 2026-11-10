/* pcib.c - the PCI-ISA bridge (class bridge, subclass ISA), pcib0: it
   offers the ISA bus, isa0, its place in the tree.  The bus is attached
   when the PCI bus has been walked (config_defer), as OpenBSD's: the
   devices of the bus come after the PCI ones. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/isa/isavar.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

static int pcib_match(struct device *parent, void *match, void *aux) {
  struct pci_dev *d = aux;
  (void)parent;
  (void)match;
  return PCI_CLASS(d->class) == PCI_CLASS_BRIDGE && PCI_SUBCLASS(d->class) == PCI_SUBCLASS_BRIDGE_ISA;
}

static void pcib_callback(struct device *self) {
  struct isabus_attach_args iba = {"isa"};
  config_found(self, &iba, NULL);
}

static void pcib_attach(struct device *parent, struct device *self, void *aux) {
  (void)parent;
  (void)aux;
  printf("\n");
  config_defer(self, pcib_callback);
}

struct cfdriver pcib_cd = {NULL, "pcib", DV_DULL, 0};
struct cfattach pcib_ca = {sizeof(struct device), pcib_match, pcib_attach};
