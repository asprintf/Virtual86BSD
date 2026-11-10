/* subr_disk.c - disks as autoconf devices, named as macOS's: disk0, disk1 ...
   in the order they attach, whichever controller they are on (pciide,
   vioblk), as OpenBSD's sd is whichever bus; disk0s1, disk0s2 ... the
   partitions of a disk (numbers from 1) that its MBR or GPT tells; rdisk0,
   rdisk0s1 ... the raw devices: character devices that read and write
   the disk's sectors, with no buffer cache.

   The block devices are in /dev as disk0, disk0s1; a partition is a block
   device of its own, made from the disk's (its sectors from an offset), so
   a file system on it has its own buffers: nothing should write disk0
   while one of its partitions is mounted.  The device numbers are
   Linux's sd: major 8, minor unit * 16 + partition; the raw ones are major
   RDISK_MAJOR with the same minors.

   The table is read when the disk attaches, and the lines of dmesg are
   OpenBSD's:

     disk0: 512MB, 512 bytes/sector, 1048576 sectors
     disk0: MBR, 1 partition
   A GPT is used if the MBR is a protective one (type 0xee); its headers
   and entry array must have good CRC-32s, else it says so and there are
   no partitions. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/file.h>
#include <uvm/uvm.h>

#define DISK_MAJOR 8
#define MAXPARTS 15 /* the minors of a disk: unit * 16 + 0 (the disk), 1 .. 15 */

/* ---- the partition table (a function of the sectors: no I/O) ---- */

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t le64(const uint8_t *p) { return (uint64_t)le32(p) | (uint64_t)le32(p + 4) << 32; }

uint32_t crc32(const void *buf, size_t n) {
  static uint32_t table[256];
  if (!table[1])
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i;
      for (int k = 0; k < 8; k++)
        c = c & 1 ? 0xedb88320U ^ (c >> 1) : c >> 1;
      table[i] = c;
    }
  const uint8_t *p = buf;
  uint32_t c = 0xffffffffU;
  while (n--)
    c = table[(c ^ *p++) & 255] ^ (c >> 8);
  return ~c;
}

/* the 4 primary entries of the MBR in sector 0: ptab_mbr: how many partitions
   there are (0: not an MBR), and 1 if the table is a protective one (a GPT
   follows) */
int ptab_mbr(const uint8_t *s0, uint64_t disksectors, struct ptab_entry *e, bool *protective) {
  *protective = false;
  if (s0[510] != 0x55 || s0[511] != 0xaa)
    return 0;
  int n = 0;
  for (int i = 0; i < 4; i++) {
    const uint8_t *p = s0 + 446 + 16 * i;
    uint8_t type = p[4];
    uint64_t start = le32(p + 8), len = le32(p + 12);
    if (!type || !len)
      continue;
    if (type == 0xee) {
      *protective = true;
      continue;
    }
    if (start + len > disksectors)
      continue; /* out of the disk: not a table of this disk */
    e[n].index = i + 1;
    e[n].start = start;
    e[n].sectors = len;
    e[n].type = type;
    n++;
  }
  return n;
}

/* the GPT: header (sector 1) and the entries (read by get, the sectors
   from lba), or a message in *why.  Entries with an all-zero type are
   unused; the partitions keep their number in the array (nr 1 is the
   first entry) */
int ptab_gpt(const uint8_t *hdr, uint64_t disksectors, int (*get)(void *arg, uint64_t lba, unsigned n, void *buf),
             void *arg, struct ptab_entry *e, int maxe, const char **why) {
  if (memcmp(hdr, "EFI PART", 8)) {
    *why = "no GPT header";
    return -1;
  }
  uint32_t hsize = le32(hdr + 12);
  if (hsize < 92 || hsize > 512) {
    *why = "bad GPT header size";
    return -1;
  }
  uint8_t h[512];
  memcpy(h, hdr, hsize);
  uint32_t crc = le32(h + 16);
  memset(h + 16, 0, 4);
  if (crc32(h, hsize) != crc) {
    *why = "GPT header checksum is bad";
    return -1;
  }
  uint64_t lba = le64(hdr + 72);
  uint32_t nent = le32(hdr + 80), esize = le32(hdr + 84), ecrc = le32(hdr + 88);
  if (esize < 128 || esize > 512 || (esize & (esize - 1)) || nent == 0 || nent > 128) {
    *why = "bad GPT entry array";
    return -1;
  }
  unsigned nsect = (nent * esize + 511) / 512;
  if (lba + nsect > disksectors) {
    *why = "GPT entries beyond the disk";
    return -1;
  }
  uint8_t *a = kmalloc((size_t)nsect * 512);
  if (!a) {
    *why = "out of memory";
    return -1;
  }
  int n = -1;
  if (get(arg, lba, nsect, a)) {
    *why = "cannot read the GPT entries";
  } else if (crc32(a, (size_t)nent * esize) != ecrc) {
    *why = "GPT entries checksum is bad";
  } else {
    uint64_t first = le64(hdr + 40), last = le64(hdr + 48);
    n = 0;
    for (uint32_t i = 0; i < nent; i++) {
      const uint8_t *p = a + (size_t)i * esize;
      bool used = false;
      for (int k = 0; k < 16; k++)
        used |= p[k] != 0;
      uint64_t s = le64(p + 32), t = le64(p + 40);
      if (!used || t < s || s < first || t > last || t >= disksectors)
        continue;
      if (n < maxe) {
        e[n].index = (int)i + 1;
        e[n].start = s;
        e[n].sectors = t - s + 1;
        e[n].type = 0;
        n++;
      }
    }
  }
  kfree(a);
  return n;
}

/* ---- a partition: a block device of the disk's sectors from an offset ---- */

struct part {
  struct blockdev *disk;
  uint64_t start;
};

static int part_read(struct blockdev *b, uint64_t sector, unsigned count, void *buf) {
  struct part *p = b->priv;
  if (sector + count > b->sectors)
    return -EIO;
  return p->disk->read(p->disk, p->start + sector, count, buf);
}

static int part_write(struct blockdev *b, uint64_t sector, unsigned count, const void *buf) {
  struct part *p = b->priv;
  if (sector + count > b->sectors)
    return -EIO;
  return p->disk->write(p->disk, p->start + sector, count, buf);
}

static int part_flush(struct blockdev *b) {
  struct part *p = b->priv;
  return p->disk->flush ? p->disk->flush(p->disk) : 0;
}

static void part_make(struct blockdev *disk, int unit, int nr, uint64_t start, uint64_t sectors) {
  struct blockdev *b = kzalloc(sizeof(*b));
  struct part *p = kzalloc(sizeof(*p));
  if (!b || !p)
    return;
  p->disk = disk;
  p->start = start;
  snprintf(b->name, sizeof(b->name), "%ss%d", disk->name, nr);
  b->dev = MKDEV(DISK_MAJOR, (unsigned)(unit * 16 + nr));
  b->sectors = sectors;
  b->read = part_read;
  b->write = part_write;
  b->flush = part_flush;
  b->priv = p;
  blockdev_register(b);
}

static int read_disk(void *arg, uint64_t lba, unsigned n, void *buf) {
  struct blockdev *b = arg;
  return b->read(b, lba, n, buf);
}

static void disk_partitions(struct blockdev *b, int unit, const char *xname) {
  struct ptab_entry e[MAXPARTS + 1];
  uint8_t s[1024];
  bool prot;
  const char *why = NULL;
  if (b->sectors < 2 || read_disk(b, 0, 2, s))
    return;
  int n = ptab_mbr(s, b->sectors, e, &prot), nm = n;
  const char *kind = "MBR";
  if (prot) {
    kind = "GPT";
    n = ptab_gpt(s + 512, b->sectors, read_disk, b, e, MAXPARTS, &why);
    if (n < 0) {
      printf("%s: %s\n", xname, why);
      return;
    }
  } else if (!nm)
    return;
  printf("%s: %s, %d partition%s\n", xname, kind, n, n == 1 ? "" : "s");
  for (int i = 0; i < n; i++)
    if (e[i].index <= MAXPARTS)
      part_make(b, unit, e[i].index, e[i].start, e[i].sectors);
}

/* ---- the disk as a device ---- */

static void rdisk_register(void);

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
  snprintf(b->name, sizeof(b->name), "%s", self->dv_xname);
  b->dev = MKDEV(DISK_MAJOR, (unsigned)(self->dv_unit * 16));
  printf(": <%s>\n", da->da_desc);
  printf("%s: %luMB, 512 bytes/sector, %lu sectors\n", self->dv_xname, (b->sectors + 1024) / 2048, b->sectors);
  blockdev_register(b);
  rdisk_register();
  disk_partitions(b, self->dv_unit, self->dv_xname);
}

struct cfdriver disk_cd = {NULL, "disk", DV_DISK, 0};
struct cfattach disk_ca = {sizeof(struct device), disk_match, disk_attach};

/* ---- the raw devices: rdisk0, rdisk0s1 ---- */

/* the sectors straight to or from the driver, in pieces that the kernel
   has a buffer for; the offset and the length are whole sectors */
static ssize_t raw_rw(struct file *f, char *buf, size_t n, off_t *pos, bool write) {
  struct blockdev *b = f->priv;
  uint64_t size = b->sectors * 512;
  if (*pos < 0 || ((uint64_t)*pos | n) & 511)
    return -EINVAL;
  if ((uint64_t)*pos >= size)
    return write && n ? -ENOSPC : 0;
  n = (size_t)MIN((uint64_t)n, size - (uint64_t)*pos);
  enum { CHUNK = 65536 };
  uint8_t *tmp = kmalloc(CHUNK);
  if (!tmp)
    return -ENOMEM;
  size_t done = 0;
  int err = 0;
  while (done < n) {
    size_t c = MIN(n - done, (size_t)CHUNK);
    uint64_t sector = (uint64_t)*pos / 512;
    if (write) {
      if (copy_from_user(tmp, buf + done, c)) {
        err = -EFAULT;
        break;
      }
      err = b->write(b, sector, (unsigned)(c / 512), tmp);
      b->unflushed = true;
    } else {
      err = b->read(b, sector, (unsigned)(c / 512), tmp);
      if (!err && copy_to_user(buf + done, tmp, c))
        err = -EFAULT;
    }
    if (err)
      break;
    done += c;
    *pos += (off_t)c;
  }
  kfree(tmp);
  return done ? (ssize_t)done : err;
}

static ssize_t raw_read(struct file *f, char *buf, size_t n, off_t *pos) { return raw_rw(f, buf, n, pos, false); }
static ssize_t raw_write(struct file *f, const char *buf, size_t n, off_t *pos) {
  return raw_rw(f, (char *)buf, n, pos, true);
}
static int raw_fsync(struct file *f) {
  struct blockdev *b = f->priv;
  return b->flush ? b->flush(b) : 0;
}
static const struct file_ops raw_fops = {.read = raw_read, .write = raw_write, .fsync = raw_fsync, .seekable = true};

static int rdisk_open(struct inode *ino, struct file *f) {
  struct blockdev *b = blockdev_get(MKDEV(DISK_MAJOR, MINOR(ino->rdev)));
  if (!b)
    return -ENXIO;
  f->priv = b;
  f->fops = &raw_fops;
  return 0;
}
static const struct chrdev rdisk_dev = {"rdisk", rdisk_open};

/* the first disk makes the raw devices there (devtmpfs has /dev/rdisk* of
   every block device then) */
bool rdisk_present;
static void rdisk_register(void) {
  if (!rdisk_present) {
    chrdev_register(RDISK_MAJOR, &rdisk_dev);
    rdisk_present = true;
  }
}
