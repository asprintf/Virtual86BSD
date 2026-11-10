/* virtiovar.h - virtio devices (dev/pv/virtio.c): a virtio device found on
   PCI, and what the driver for its kind is offered. */
#ifndef _DEV_PV_VIRTIOVAR_H_
#define _DEV_PV_VIRTIOVAR_H_

#include <dev/pci/pcivar.h>

/* the kinds of device (the PCI subsystem ID of a legacy device) */
#define VIRTIO_ID_NETWORK 1
#define VIRTIO_ID_BLOCK 2
#define VIRTIO_ID_CONSOLE 3
#define VIRTIO_ID_ENTROPY 4
#define VIRTIO_ID_BALLOON 5
#define VIRTIO_ID_SCSI 8

/* the aux of a device at virtio */
struct virtio_attach_args {
  struct pci_dev *va_pd; /* the PCI device (its BAR 0, the I/O ports) */
  int va_devid;          /* VIRTIO_ID_ */
};

#endif
