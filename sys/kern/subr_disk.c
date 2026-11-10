/* subr_disk.c - disks as autoconf devices: disk0, disk1 ... in the order they
   attach, whichever controller they are on (pciide, vioblk), as OpenBSD's
   sd is whichever bus.  (The block devices keep the names of Linux's -- sda,
   vda -- for now; the line says which.) */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/disk.h>

static int disk_match(struct device *parent, void *match, void *aux) {
  (void)parent;
  (void)match;
  (void)aux;
  return 1; /* only a controller offers one */
}

static void disk_attach(struct device *parent, struct device *self, void *aux) {
  struct disk_attach_args *da = aux;
  struct blockdev *b = da->da_bd;
  (void)parent;
  printk(": <%s>\n", da->da_desc);
  printk("%s: /dev/%s\n", self->dv_xname, b->name);
  blockdev_register(b);
}

struct cfdriver disk_cd = {NULL, "disk", DV_DISK, 0};
struct cfattach disk_ca = {sizeof(struct device), disk_match, disk_attach};
