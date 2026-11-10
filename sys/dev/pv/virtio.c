/* virtio.c - the virtio transport on PCI: the legacy ("transitional")
   interface, vendor 1af4 devices 1000..103f, registers in I/O BAR 0.  What
   kind of device it is, is in the subsystem ID; the driver for that kind
   (vioblk) attaches at virtio. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pv/virtiovar.h>

static int virtio_match(struct device *parent, void *match, void *aux) {
  struct pci_dev *d = aux;
  (void)parent;
  (void)match;
  return d->vendor == PCI_VENDOR_QUMRANET && d->device >= 0x1000 && d->device <= 0x103f && (d->bar[0] & 1);
}

static const char *virtio_devname(int id) {
  switch (id) {
  case VIRTIO_ID_NETWORK: return "network";
  case VIRTIO_ID_BLOCK: return "block";
  case VIRTIO_ID_CONSOLE: return "console";
  case VIRTIO_ID_ENTROPY: return "entropy";
  case VIRTIO_ID_BALLOON: return "memory balloon";
  case VIRTIO_ID_SCSI: return "SCSI";
  }
  return "unknown";
}

static int virtio_print(void *aux, const char *pnp) {
  struct virtio_attach_args *va = aux;
  if (pnp)
    printf("virtio %s (%d) at %s", virtio_devname(va->va_devid), va->va_devid, pnp);
  return UNCONF;
}

static void virtio_attach(struct device *parent, struct device *self, void *aux) {
  struct pci_dev *d = aux;
  struct virtio_attach_args va;
  (void)parent;
  printf("\n");
  va.va_pd = d;
  va.va_devid = (int)(pci_read(d, 0x2c) >> 16); /* subsystem ID */
  config_found(self, &va, virtio_print);
}

struct cfdriver virtio_cd = {NULL, "virtio", DV_DULL, 0};
struct cfattach virtio_ca = {sizeof(struct device), virtio_match, virtio_attach};
