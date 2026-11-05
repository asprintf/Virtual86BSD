/* virtio_blk.c - virtio block devices (QEMU's -drive if=virtio): vda, vdb,
   ... as on Linux.

   The legacy ("transitional") PCI interface: registers in I/O BAR 0, one
   virtqueue in physically contiguous pages.  A request is a chain of
   descriptors -- a header (type, sector), the data (in pieces that do not
   cross a page), a status byte -- put on the available ring, and a write
   of the queue's number to the notify register; the device puts it on the
   used ring when it is done.  The driver waits for that by watching the
   used ring in memory (with interrupts on, and with the device's
   interrupts off): no port I/O while waiting, so under KVM a request costs
   one exit, where an ATA sector costs several.  (It does not sleep: the
   kernel's file systems expect a transfer to complete without other
   tasks running meanwhile.)

   Writes go to the device's cache; the flush (VIRTIO_BLK_T_FLUSH, when the
   device offers it) is the buffer cache's, at sync. */
#include "kernel.h"
#include "arch.h"
#include "mm.h"
#include "dev.h"

/* legacy registers (offsets in BAR 0) */
#define R_DEV_FEATURES   0x00
#define R_GUEST_FEATURES 0x04
#define R_QUEUE_PFN      0x08
#define R_QUEUE_SIZE     0x0c
#define R_QUEUE_SELECT   0x0e
#define R_QUEUE_NOTIFY   0x10
#define R_STATUS         0x12
#define R_ISR            0x13
#define R_CONFIG         0x14 /* without MSI-X: capacity (64 bits, in sectors) */

#define S_ACK 1
#define S_DRIVER 2
#define S_DRIVER_OK 4
#define S_FAILED 128

#define F_FLUSH (1U << 9)

#define T_IN 0
#define T_OUT 1
#define T_FLUSH 4

#define D_NEXT 1
#define D_WRITE 2 /* the device writes this buffer */

struct vdesc {
  uint64_t addr;
  uint32_t len;
  uint16_t flags, next;
};

struct vblk {
  uint16_t io;
  unsigned qsize;
  volatile struct vdesc *desc;
  volatile uint16_t *avail; /* flags, idx, ring[qsize] */
  volatile uint16_t *used;  /* flags, idx, then {u32 id, u32 len}[qsize] */
  uint16_t avail_idx, used_seen;
  bool flush;
  /* the request's header and status, in memory the device can reach */
  volatile struct { uint32_t type, reserved; uint64_t sector; } *hdr;
  volatile uint8_t *status;
  uint64_t hdr_phys;
};

static int nvdisks;

/* the physical address of kernel memory: the direct map, or the image */
static uint64_t phys_of(const void *v) {
  uint64_t a = (uint64_t)v;
  return a >= KERNEL_BASE ? a - KERNEL_BASE : V2P(a);
}

#define MAX_SEG 20 /* 64 KiB of data in page pieces (17 at most) + header + status */

static void set_desc(struct vblk *v, unsigned n, uint64_t addr, uint32_t len, uint16_t flags) {
  v->desc[n].addr = addr;
  v->desc[n].len = len;
  v->desc[n].flags = flags;
  v->desc[n].next = (uint16_t)(n + 1);
}

/* one request: type, sector, count sectors at buf (0 for a flush) */
static int request(struct vblk *v, uint32_t type, uint64_t sector, void *buf, size_t len) {
  v->hdr->type = type;
  v->hdr->reserved = 0;
  v->hdr->sector = sector;
  *v->status = 0xff;
  unsigned n = 0;
  set_desc(v, n, v->hdr_phys, 16, D_NEXT);
  n++;
  uint8_t *p = buf;
  while (len) {
    size_t chunk = PAGE_SIZE - ((uint64_t)p & (PAGE_SIZE - 1));
    if (chunk > len)
      chunk = len;
    set_desc(v, n, phys_of(p), (uint32_t)chunk, (uint16_t)(D_NEXT | (type == T_IN ? D_WRITE : 0)));
    n++;
    p += chunk;
    len -= chunk;
  }
  set_desc(v, n, v->hdr_phys + 16, 1, D_WRITE);
  /* the chain starts at descriptor 0; one request at a time */
  v->avail[2 + v->avail_idx % v->qsize] = 0;
  mb();
  v->avail_idx++;
  v->avail[1] = v->avail_idx;
  mb();
  outw((uint16_t)(v->io + R_QUEUE_NOTIFY), 0);
  /* wait for the used ring: long enough for any disk */
  uint64_t deadline = monotonic_ns() + 30000000000UL;
  while (v->used[1] == v->used_seen) {
    if (monotonic_ns() > deadline) {
      printk("virtio-blk: request timed out\n");
      return -EIO;
    }
    cpu_pause();
  }
  mb();
  v->used_seen++;
  return *v->status == 0 ? 0 : -EIO;
}

static int vblk_rw(struct blockdev *b, uint64_t sector, unsigned count, void *buf, bool write) {
  struct vblk *v = b->priv;
  if (sector + count > b->sectors)
    return -EIO;
  uint8_t *p = buf;
  while (count) {
    unsigned n = count > 128 ? 128 : count; /* 64 KiB */
    int e = request(v, write ? T_OUT : T_IN, sector, p, (size_t)n * 512);
    if (e)
      return e;
    sector += n;
    count -= n;
    p += (size_t)n * 512;
  }
  return 0;
}

static int vblk_read(struct blockdev *b, uint64_t s, unsigned c, void *buf) { return vblk_rw(b, s, c, buf, false); }
static int vblk_write(struct blockdev *b, uint64_t s, unsigned c, const void *buf) {
  return vblk_rw(b, s, c, (void *)buf, true);
}
static int vblk_flush(struct blockdev *b) {
  struct vblk *v = b->priv;
  return v->flush ? request(v, T_FLUSH, 0, NULL, 0) : 0;
}

static void probe(struct pci_dev *d) {
  if (!(d->bar[0] & 1)) {
    printk("virtio-blk: no I/O BAR\n");
    return;
  }
  uint16_t io = (uint16_t)(d->bar[0] & ~3U);
  pci_write(d, 0x04, pci_read(d, 0x04) | 0x5); /* I/O space, bus master */
  outb((uint16_t)(io + R_STATUS), 0);         /* reset */
  outb((uint16_t)(io + R_STATUS), S_ACK);
  outb((uint16_t)(io + R_STATUS), S_ACK | S_DRIVER);
  uint32_t feat = inl((uint16_t)(io + R_DEV_FEATURES));
  outl((uint16_t)(io + R_GUEST_FEATURES), feat & F_FLUSH);
  outw((uint16_t)(io + R_QUEUE_SELECT), 0);
  unsigned qsize = inw((uint16_t)(io + R_QUEUE_SIZE));
  if (qsize < MAX_SEG) {
    outb((uint16_t)(io + R_STATUS), S_FAILED);
    printk("virtio-blk: queue of %u entries\n", qsize);
    return;
  }
  /* the queue: descriptors and available ring, then (page-aligned) the used ring */
  size_t part1 = 16 * qsize + 6 + 2 * qsize;
  size_t total = PAGE_UP(part1) + PAGE_UP(6 + 8 * qsize);
  int order = 0;
  while ((PAGE_SIZE << order) < total)
    order++;
  uint64_t q = alloc_pages(order);
  uint64_t hp = alloc_zeroed_page();
  struct vblk *v = kzalloc(sizeof(*v));
  if (!q || !hp || !v) {
    outb((uint16_t)(io + R_STATUS), S_FAILED);
    printk("virtio-blk: out of memory\n");
    return;
  }
  memset(P2V(q), 0, PAGE_SIZE << order);
  v->io = io;
  v->qsize = qsize;
  v->desc = (volatile struct vdesc *)P2V(q);
  v->avail = (volatile uint16_t *)((uint8_t *)P2V(q) + 16 * qsize);
  v->used = (volatile uint16_t *)((uint8_t *)P2V(q) + PAGE_UP(part1));
  v->avail[0] = 1; /* VRING_AVAIL_F_NO_INTERRUPT: we watch the used ring */
  v->flush = (feat & F_FLUSH) != 0;
  v->hdr = (void *)P2V(hp);
  v->status = (volatile uint8_t *)P2V(hp) + 16;
  v->hdr_phys = hp;
  outl((uint16_t)(io + R_QUEUE_PFN), (uint32_t)(q >> 12));
  outb((uint16_t)(io + R_STATUS), S_ACK | S_DRIVER | S_DRIVER_OK);
  uint64_t sectors = inl((uint16_t)(io + R_CONFIG)) | (uint64_t)inl((uint16_t)(io + R_CONFIG + 4)) << 32;
  struct blockdev *b = kzalloc(sizeof(*b));
  snprintf(b->name, sizeof(b->name), "vd%c", 'a' + nvdisks);
  b->dev = MKDEV(254, nvdisks * 16);
  b->sectors = sectors;
  b->read = vblk_read;
  b->write = vblk_write;
  b->flush = vblk_flush;
  b->priv = v;
  nvdisks++;
  blockdev_register(b);
}

void virtio_blk_init(void) {
  /* 1af4:1001, the legacy (transitional) block device; one is enough for
     now (pci_find gives the first) */
  static const uint16_t ids[] = {0x1af4, 0x1001, 0, 0};
  struct pci_dev d;
  if (pci_find(ids, &d))
    probe(&d);
}
