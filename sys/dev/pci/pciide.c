/* pciide.c - ATA disks on the legacy IDE ports of a PCI IDE controller,
   PIO with polling, 28- and 48-bit LBA.  The disks are devices (disk0,
   disk1 ...: kern/subr_disk.c), at the channel and drive.

   Interrupts stay on while the driver polls (the drive's own interrupt is
   off), so the clock keeps time and the terminals take their input while
   the disk works; and the drive's write cache is flushed by the buffer
   cache's sync (sync, fsync, the periodic flush), not after every write
   (under QEMU a flush is an fdatasync on the host, which can take
   milliseconds). */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>

struct ata {
  uint16_t base, ctl;
  int slave;
  bool lba48;
};

static int wait_ready(struct ata *a) {
  for (int i = 0; i < 10000000; i++) {
    uint8_t s = inb(a->base + 7);
    if (s & 0x01)
      return -EIO; /* ERR */
    if (!(s & 0x80) && (s & 0x08))
      return 0; /* !BSY, DRQ */
  }
  return -EIO;
}

static int wait_idle(struct ata *a) {
  for (int i = 0; i < 10000000; i++) {
    uint8_t s = inb(a->base + 7);
    if (!(s & 0x80))
      return (s & 0x21) ? -EIO : 0;
  }
  return -EIO;
}

static void select_lba(struct ata *a, uint64_t lba, unsigned count, bool write) {
  if (a->lba48) {
    outb(a->base + 6, (uint8_t)(0x40 | a->slave << 4));
    outb(a->base + 2, (uint8_t)(count >> 8));
    outb(a->base + 3, (uint8_t)(lba >> 24));
    outb(a->base + 4, (uint8_t)(lba >> 32));
    outb(a->base + 5, (uint8_t)(lba >> 40));
    outb(a->base + 2, (uint8_t)count);
    outb(a->base + 3, (uint8_t)lba);
    outb(a->base + 4, (uint8_t)(lba >> 8));
    outb(a->base + 5, (uint8_t)(lba >> 16));
    outb(a->base + 7, write ? 0x34 : 0x24);
  } else {
    outb(a->base + 6, (uint8_t)(0xe0 | a->slave << 4 | ((lba >> 24) & 0x0f)));
    outb(a->base + 2, (uint8_t)count);
    outb(a->base + 3, (uint8_t)lba);
    outb(a->base + 4, (uint8_t)(lba >> 8));
    outb(a->base + 5, (uint8_t)(lba >> 16));
    outb(a->base + 7, write ? 0x30 : 0x20);
  }
}

static int ata_rw(struct blockdev *b, uint64_t sector, unsigned count, void *buf, bool write) {
  struct ata *a = b->priv;
  uint8_t *p = buf;
  if (sector + count > b->sectors)
    return -EIO;
  while (count) {
    unsigned n = count > 128 ? 128 : count;
    if (wait_idle(a))
      return -EIO;
    select_lba(a, sector, n, write);
    for (unsigned i = 0; i < n; i++) {
      if (wait_ready(a))
        return -EIO;
      if (write)
        outsw(a->base, p, 256);
      else
        insw(a->base, p, 256);
      p += 512;
    }
    if (write && wait_idle(a))
      return -EIO;
    sector += n;
    count -= n;
  }
  return 0;
}

static int ata_flush(struct blockdev *b) {
  struct ata *a = b->priv;
  if (wait_idle(a))
    return -EIO;
  outb(a->base + 6, (uint8_t)((a->lba48 ? 0x40 : 0xe0) | a->slave << 4));
  outb(a->base + 7, a->lba48 ? 0xea : 0xe7); /* FLUSH CACHE (EXT) */
  return wait_idle(a);
}

static int ata_read(struct blockdev *b, uint64_t s, unsigned c, void *buf) { return ata_rw(b, s, c, buf, false); }
static int ata_write(struct blockdev *b, uint64_t s, unsigned c, const void *buf) {
  return ata_rw(b, s, c, (void *)buf, true);
}

/* a disk on a channel: identified (ATA IDENTIFY), its block device made
   (not registered: the disk device does it) or NULL if there is none */
static struct blockdev *ata_probe(uint16_t base, uint16_t ctl, int slave, char *model, size_t msize) {
  if (inb(base + 7) == 0xff)
    return NULL; /* no controller */
  outb(base + 6, (uint8_t)(0xa0 | slave << 4));
  for (int i = 0; i < 15; i++)
    inb(base + 7);
  outb(base + 2, 0);
  outb(base + 3, 0);
  outb(base + 4, 0);
  outb(base + 5, 0);
  outb(base + 7, 0xec); /* IDENTIFY */
  if (inb(base + 7) == 0)
    return NULL;
  int i;
  for (i = 0; i < 1000000 && (inb(base + 7) & 0x80); i++)
    ;
  if (inb(base + 4) || inb(base + 5))
    return NULL; /* ATAPI or SATA: not a plain ATA disk */
  for (i = 0; i < 1000000; i++) {
    uint8_t s = inb(base + 7);
    if (s & 1)
      return NULL;
    if (s & 8)
      break;
  }
  if (i == 1000000)
    return NULL;
  uint16_t id[256];
  insw(base, id, 256);
  struct ata *a = kzalloc(sizeof(*a));
  a->base = base;
  a->ctl = ctl;
  a->slave = slave;
  uint64_t sectors = (uint64_t)id[60] | (uint64_t)id[61] << 16;
  if (id[83] & (1 << 10)) {
    a->lba48 = true;
    sectors = (uint64_t)id[100] | (uint64_t)id[101] << 16 | (uint64_t)id[102] << 32 | (uint64_t)id[103] << 48;
  }
  /* the model: words 27-46, two characters each, the first in the high byte */
  size_t n = 0;
  for (int w = 27; w <= 46 && n + 2 < msize; w++) {
    model[n++] = (char)(id[w] >> 8);
    model[n++] = (char)id[w];
  }
  while (n > 0 && (model[n - 1] == ' ' || model[n - 1] == 0))
    n--;
  model[n] = 0;
  struct blockdev *b = kzalloc(sizeof(*b));
  b->sectors = sectors;
  b->read = ata_read;
  b->write = ata_write;
  b->flush = ata_flush;
  b->priv = a;
  outb(ctl, 0x02); /* no interrupts: we poll */
  return b;
}

/* the aux of a disk at pciide */
struct pciide_disk_args {
  struct disk_attach_args da; /* first */
  int channel, drive;
};

static int pciide_disk_print(void *aux, const char *pnp) {
  struct pciide_disk_args *a = aux;
  if (pnp)
    printf("disk at %s", pnp);
  printf(" channel %d drive %d", a->channel, a->drive);
  return QUIET;
}

static bool pciide_attached;

/* the controller is the PIIX's IDE function, or any other in compatibility
   mode: the class says so.  Its channels are the legacy ports; one controller
   is all there is (the ports are not the controller's own) */
static int pciide_match(struct device *parent, void *match, void *aux) {
  struct pci_dev *d = aux;
  (void)parent;
  (void)match;
  return !pciide_attached && PCI_CLASS(d->class) == PCI_CLASS_MASS_STORAGE &&
         PCI_SUBCLASS(d->class) == PCI_SUBCLASS_MASS_STORAGE_IDE;
}

static void pciide_attach(struct device *parent, struct device *self, void *aux) {
  static const struct {
    uint16_t base, ctl;
  } channel[2] = {{0x1f0, 0x3f6}, {0x170, 0x376}};
  (void)parent;
  (void)aux;
  pciide_attached = true;
  printf("\n");
  for (int c = 0; c < 2; c++)
    for (int drive = 0; drive < 2; drive++) {
      char model[48];
      struct blockdev *b = ata_probe(channel[c].base, channel[c].ctl, drive, model, sizeof(model));
      if (!b)
        continue;
      struct pciide_disk_args a = {{b, model}, c, drive};
      config_found(self, &a, pciide_disk_print);
    }
}

struct cfdriver pciide_cd = {NULL, "pciide", DV_DULL, 0};
struct cfattach pciide_ca = {sizeof(struct device), pciide_match, pciide_attach};
