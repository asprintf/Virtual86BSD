/* mainbus.c - the root of the device tree: the processor and the buses
   (PCI first, then ISA, as a PC's firmware sees them). */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <machine/mainbus.h>

static int mainbus_print(void *aux, const char *pnp) {
  struct mainbus_attach_args *ma = aux;
  if (pnp)
    printk("%s at %s", ma->maa_name, pnp);
  if (!strcmp(ma->maa_name, "pci"))
    printk(" bus %d", ma->maa_bus);
  return UNCONF;
}

static int mainbus_match(struct device *parent, void *match, void *aux) {
  (void)parent;
  (void)match;
  (void)aux;
  return 1;
}

static void mainbus_attach(struct device *parent, struct device *self, void *aux) {
  (void)parent;
  (void)aux;
  struct mainbus_attach_args ma = {"cpu", 0};
  printk("\n");
  config_found(self, &ma, mainbus_print);
  ma.maa_name = "pci";
  config_found(self, &ma, mainbus_print);
  ma.maa_name = "isa";
  config_found(self, &ma, mainbus_print);
}

struct cfdriver mainbus_cd = {NULL, "mainbus", DV_DULL, 0};
struct cfattach mainbus_ca = {sizeof(struct device), mainbus_match, mainbus_attach};
