/* wsdisplay.c - the console display as a device, wsdisplay0: it only says
   what the console is (the VGA text screen with a vt100 terminal, /dev/ttyC0,
   is vga.c and kern/tty.c), and the keyboard it is used with. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

extern struct cfdriver wskbd_cd;

static int wsdisplay_match(struct device *parent, void *match, void *aux) {
  (void)parent;
  (void)match;
  (void)aux;
  return 1; /* only a display (vga) offers one */
}

static void wsdisplay_attach(struct device *parent, struct device *self, void *aux) {
  (void)parent;
  (void)self;
  (void)aux;
  printf(": console (std, vt100 emulation)");
  if (wskbd_cd.cd_ndevs > 0 && wskbd_cd.cd_devs[0])
    printf(", using %s", wskbd_cd.cd_devs[0]->dv_xname);
  printf("\n");
}

struct cfdriver wsdisplay_cd = {NULL, "wsdisplay", DV_DULL, 0};
struct cfattach wsdisplay_ca = {sizeof(struct device), wsdisplay_match, wsdisplay_attach};
