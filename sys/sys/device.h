/* device.h - autoconfiguration: devices, drivers, and how they find each
   other (kern/subr_autoconf.c; the table is ioconf.c, made by config(8)). */
#ifndef _SYS_DEVICE_H_
#define _SYS_DEVICE_H_

#include <sys/systm.h>

enum devclass { DV_DULL, DV_CPU, DV_DISK, DV_IFNET, DV_TTY };

/* a device found and attached: com0, em0 ... (a driver's own state follows it
   when ca_devsize is larger) */
struct device {
  enum devclass dv_class;
  struct cfdata *dv_cfdata;     /* what it was made from */
  struct cfdriver *dv_cfdriver;
  struct cfattach *dv_cfattach;
  int dv_unit;
  char dv_xname[16];            /* "com0" */
  struct device *dv_parent;     /* NULL: mainbus */
};

/* the driver: what its devices have in common.  cd_devs[unit] is the device */
struct cfdriver {
  struct device **cd_devs;
  const char *cd_name;          /* "com" */
  enum devclass cd_class;
  int cd_ndevs;
};

/* its way to attach.  match: > 0 if the driver is for what aux describes
   (the best match wins); attach: set the device up, print what is
   particular to it and end the line */
struct cfattach {
  size_t ca_devsize;
  int (*ca_match)(struct device *parent, void *match, void *aux);
  void (*ca_attach)(struct device *parent, struct device *self, void *aux);
};

/* one line of the table: this driver, so numbered, at that parent.  The
   locators are those of the parent's bus (isa: port, irq ...), in the order
   config(8) numbered them (locators.h) */
struct cfdata {
  struct cfattach *cf_attach;
  struct cfdriver *cf_driver;
  short cf_unit;
  short cf_fstate;              /* FSTATE_ */
  int *cf_loc;
  int cf_flags;
  struct cfdriver *cf_parent;   /* NULL: root */
  short cf_punit;               /* -1: any unit of the parent */
  short cf_starunit1;           /* a star line: the unit to look from next */
};
#define FSTATE_NORMAL 0         /* not attached yet: one device, this unit */
#define FSTATE_FOUND 1          /* attached */
#define FSTATE_STAR 2           /* as many as found, any unit */

extern struct cfdata cfdata[];  /* ioconf.c: ends with cf_attach == NULL */

/* what a bus does to tell of a device: pnp is the parent's name when the
   device found no driver, else NULL.  (The words after "xx0 at yy0".) */
typedef int (*cfprint_t)(void *aux, const char *pnp);
typedef int (*cfmatch_t)(struct device *parent, void *match, void *aux);
#define QUIET 0
#define UNCONF 1                /* print " not configured" if no driver found */

struct device *config_rootfound(const char *name, void *aux);
struct device *config_found(struct device *parent, void *aux, cfprint_t print);
struct device *config_found_sm(struct device *parent, void *aux, cfprint_t print, cfmatch_t submatch);
struct cfdata *config_search(cfmatch_t submatch, struct device *parent, void *aux);
int config_match(struct device *parent, void *match, void *aux);
struct device *config_attach(struct device *parent, struct cfdata *cf, void *aux, cfprint_t print);
void config_scan(cfmatch_t scanner, struct device *parent);
/* fn(dev) is called once the devices found so far are attached (a bridge:
   its child bus comes after the devices of the bus it is on) */
void config_defer(struct device *dev, void (*fn)(struct device *));
void config_process_deferred(void);

#endif
