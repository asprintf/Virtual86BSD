/* isa.c - the ISA bus.  Nothing says what is on it: the table does.  Each
   line of devices at isa (com0 at isa0 port 0x3f8 irq 4) is given to the
   driver's match, with the locators of the line in an isa_attach_args, and
   the driver says if the device is there. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/isa/isavar.h>
#include <locators.h>

/* the place of a device on the bus: port, memory, interrupt */
static int isa_print(void *aux, const char *pnp) {
  struct isa_attach_args *ia = aux;
  if (pnp)
    return UNCONF;
  if (ia->ia_iosize)
    printf(" port 0x%x/%d", ia->ia_iobase, ia->ia_iosize);
  else if (ia->ia_iobase != -1)
    printf(" port 0x%x", ia->ia_iobase);
  if (ia->ia_msize)
    printf(" iomem 0x%x/%d", ia->ia_maddr, ia->ia_msize);
  else if (ia->ia_maddr != -1)
    printf(" iomem 0x%x", ia->ia_maddr);
  if (ia->ia_irq != -1)
    printf(" irq %d", ia->ia_irq);
  return QUIET;
}

static int isa_scan(struct device *parent, void *match, void *aux) {
  struct cfdata *cf = match;
  struct isa_attach_args ia;
  (void)aux;
  ia.ia_iobase = cf->cf_loc[ISACF_PORT];
  ia.ia_iosize = 0;
  ia.ia_irq = cf->cf_loc[ISACF_IRQ];
  ia.ia_maddr = cf->cf_loc[ISACF_IOMEM];
  ia.ia_msize = cf->cf_loc[ISACF_IOSIZ];
  if (config_match(parent, cf, &ia) > 0)
    config_attach(parent, cf, &ia, isa_print);
  return 0;
}

static int isa_match(struct device *parent, void *match, void *aux) {
  struct isabus_attach_args *iba = aux;
  (void)parent;
  (void)match;
  return !strcmp(iba->iba_name, "isa");
}

static void isa_attach(struct device *parent, struct device *self, void *aux) {
  (void)parent;
  (void)aux;
  printf("\n");
  config_scan(isa_scan, self);
}

struct cfdriver isa_cd = {NULL, "isa", DV_DULL, 0};
struct cfattach isa_ca = {sizeof(struct device), isa_match, isa_attach};
