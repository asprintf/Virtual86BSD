/* ata.c - ATA disks on the legacy IDE ports, PIO with polling, 28- and
   48-bit LBA.  Names as Linux's libata: sda, sdb, ...

   Interrupts stay on while the driver polls (the drive's own interrupt is
   off), so the clock keeps time and the terminals take their input while
   the disk works; and the drive's write cache is flushed by the buffer
   cache's sync (sync, fsync, the periodic flush), not after every write
   (under QEMU a flush is an fdatasync on the host, which can take
   milliseconds). */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>

struct ata {
  uint16_t base, ctl;
  int slave;
  bool lba48;
};

static int ndisks;

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

static void probe(uint16_t base, uint16_t ctl, int slave) {
  if (inb(base + 7) == 0xff)
    return; /* no controller */
  outb(base + 6, (uint8_t)(0xa0 | slave << 4));
  for (int i = 0; i < 15; i++)
    inb(base + 7);
  outb(base + 2, 0);
  outb(base + 3, 0);
  outb(base + 4, 0);
  outb(base + 5, 0);
  outb(base + 7, 0xec); /* IDENTIFY */
  if (inb(base + 7) == 0)
    return;
  int i;
  for (i = 0; i < 1000000 && (inb(base + 7) & 0x80); i++)
    ;
  if (inb(base + 4) || inb(base + 5))
    return; /* ATAPI or SATA: not a plain ATA disk */
  for (i = 0; i < 1000000; i++) {
    uint8_t s = inb(base + 7);
    if (s & 1)
      return;
    if (s & 8)
      break;
  }
  if (i == 1000000)
    return;
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
  struct blockdev *b = kzalloc(sizeof(*b));
  snprintf(b->name, sizeof(b->name), "sd%c", 'a' + ndisks);
  b->dev = MKDEV(8, ndisks * 16);
  b->sectors = sectors;
  b->read = ata_read;
  b->write = ata_write;
  b->flush = ata_flush;
  b->priv = a;
  ndisks++;
  outb(ctl, 0x02); /* no interrupts: we poll */
  blockdev_register(b);
}

void ata_init(void) {
  probe(0x1f0, 0x3f6, 0);
  probe(0x1f0, 0x3f6, 1);
  probe(0x170, 0x376, 0);
  probe(0x170, 0x376, 1);
}
