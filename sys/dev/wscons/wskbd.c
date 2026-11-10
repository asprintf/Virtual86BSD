/* wskbd.c - the console keyboard as a device, wskbd0: it only says so (the
   terminal and the input of the keyboard stay where they are, pckbc.c and
   kern/tty.c). */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

static int wskbd_match(struct device *parent, void *match, void *aux) {
  (void)parent;
  (void)match;
  (void)aux;
  return 1; /* only a keyboard (pckbd) offers one */
}

static void wskbd_attach(struct device *parent, struct device *self, void *aux) {
  (void)parent;
  (void)self;
  (void)aux;
  printf(": console keyboard\n");
}

struct cfdriver wskbd_cd = {NULL, "wskbd", DV_DULL, 0};
struct cfattach wskbd_ca = {sizeof(struct device), wskbd_match, wskbd_attach};
