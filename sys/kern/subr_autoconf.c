/*
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

/* subr_autoconf.c - autoconfiguration, as OpenBSD's: the devices of the
   kernel are the table cfdata (ioconf.c, from config(8) and GENERIC).  A bus
   driver, when it attaches, finds what is on it and calls config_found()
   for each thing: every line of the table for a driver that attaches at
   that bus is asked to match, the best one is attached (its attach function
   runs) and the device gets its name, unit by unit: "em0".  Or the bus asks
   the table what it should look for (config_scan: the ISA bus, where nothing
   says what is there).

   What is printed is the device tree as dmesg shows it:

     em0 at pci0 dev 3 function 0 "Intel 82540EM" rev 0x03: irq 11, ...

   "em0 at pci0" is printed here, what follows by the bus's print function
   (the first call: the place), then the driver's attach (what is particular
   to it), which ends the line.  What no driver takes gets the line from the
   print function and " not configured". */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <uvm/uvm_extern.h>

/* is parent (NULL: none) where this line of the table attaches? */
static bool parent_is(const struct cfdata *cf, const struct device *parent) {
  if (!parent)
    return cf->cf_parent == NULL;
  return cf->cf_parent == parent->dv_cfdriver && (cf->cf_punit < 0 || cf->cf_punit == parent->dv_unit);
}

int config_match(struct device *parent, void *match, void *aux) {
  struct cfdata *cf = match;
  return cf->cf_attach->ca_match(parent, cf, aux);
}

/* the line of the table with the best match for aux, or NULL; submatch, if
   not NULL, is called instead of config_match (a bus: to give a line's
   locators to the driver's match) */
struct cfdata *config_search(cfmatch_t submatch, struct device *parent, void *aux) {
  struct cfdata *best = NULL;
  int pri = 0;
  for (struct cfdata *cf = cfdata; cf->cf_attach; cf++) {
    if (cf->cf_fstate == FSTATE_FOUND || !parent_is(cf, parent))
      continue;
    int r = submatch ? submatch(parent, cf, aux) : config_match(parent, cf, aux);
    if (r > pri) {
      best = cf;
      pri = r;
    }
  }
  return best;
}

/* the device cf describes, attached to parent, aux describing it */
struct device *config_attach(struct device *parent, struct cfdata *cf, void *aux, cfprint_t print) {
  struct cfdriver *cd = cf->cf_driver;
  struct cfattach *ca = cf->cf_attach;
  int unit;
  if (cf->cf_fstate == FSTATE_STAR) {
    for (unit = cf->cf_starunit1; unit < cd->cd_ndevs; unit++)
      if (!cd->cd_devs[unit])
        break;
    cf->cf_starunit1 = (short)(unit + 1);
  } else {
    unit = cf->cf_unit;
    cf->cf_fstate = FSTATE_FOUND;
  }
  if (unit < cd->cd_ndevs && cd->cd_devs[unit])
    panic("config_attach: %s%d is there already", cd->cd_name, unit);
  if (unit >= cd->cd_ndevs) {
    int n = unit + 1;
    struct device **v = krealloc(cd->cd_devs, (size_t)cd->cd_ndevs * sizeof(*v), (size_t)n * sizeof(*v));
    if (!v)
      panic("config_attach: out of memory");
    for (int i = cd->cd_ndevs; i < n; i++)
      v[i] = NULL;
    cd->cd_devs = v;
    cd->cd_ndevs = n;
  }
  struct device *dev = kzalloc(ca->ca_devsize < sizeof(*dev) ? sizeof(*dev) : ca->ca_devsize);
  if (!dev)
    panic("config_attach: out of memory");
  dev->dv_class = cd->cd_class;
  dev->dv_cfdata = cf;
  dev->dv_cfdriver = cd;
  dev->dv_cfattach = ca;
  dev->dv_unit = unit;
  dev->dv_parent = parent;
  snprintf(dev->dv_xname, sizeof(dev->dv_xname), "%s%d", cd->cd_name, unit);
  cd->cd_devs[unit] = dev;
  printf("%s at %s", dev->dv_xname, parent ? parent->dv_xname : "root");
  if (print)
    print(aux, NULL);
  ca->ca_attach(parent, dev, aux);
  return dev;
}

struct device *config_found_sm(struct device *parent, void *aux, cfprint_t print, cfmatch_t submatch) {
  struct cfdata *cf = config_search(submatch, parent, aux);
  if (cf)
    return config_attach(parent, cf, aux, print);
  if (print && print(aux, parent->dv_xname) == UNCONF)
    printf(" not configured\n");
  return NULL;
}

struct device *config_found(struct device *parent, void *aux, cfprint_t print) {
  return config_found_sm(parent, aux, print, NULL);
}

/* the first device of the table that attaches at the root and is called
   name (the mainbus) */
struct device *config_rootfound(const char *name, void *aux) {
  for (struct cfdata *cf = cfdata; cf->cf_attach; cf++)
    if (cf->cf_fstate != FSTATE_FOUND && !cf->cf_parent && !strcmp(cf->cf_driver->cd_name, name) &&
        config_match(NULL, cf, aux) > 0)
      return config_attach(NULL, cf, aux, NULL);
  return NULL;
}

/* every line of the table that attaches at parent is given to scanner,
   which probes for the device and attaches it with config_attach (a bus
   with no way to find out what is on it: ISA) */
void config_scan(cfmatch_t scanner, struct device *parent) {
  for (struct cfdata *cf = cfdata; cf->cf_attach; cf++)
    if (cf->cf_fstate != FSTATE_FOUND && parent_is(cf, parent))
      scanner(parent, cf, NULL);
}

/* what is to be done after the tree has been walked, in order (a bridge's
   bus: OpenBSD's config_defer) */
#define NDEFER 16
static struct {
  struct device *dev;
  void (*fn)(struct device *);
} deferred[NDEFER];
static int ndeferred;

void config_defer(struct device *dev, void (*fn)(struct device *)) {
  if (ndeferred == NDEFER)
    panic("config_defer: too many");
  deferred[ndeferred].dev = dev;
  deferred[ndeferred].fn = fn;
  ndeferred++;
}

/* (a deferred call may defer more: they run after those before them) */
void config_process_deferred(void) {
  for (int i = 0; i < ndeferred; i++)
    deferred[i].fn(deferred[i].dev);
  ndeferred = 0;
}
