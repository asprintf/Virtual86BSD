/* dev.h - console, character and block devices. */
#ifndef DEV_H
#define DEV_H

#include "kernel.h"

/* console output: every console device (serial, VGA) */
void console_write(const char *s, size_t n);
void serial_init(void);
void serial_write(const char *s, size_t n);
void vga_init(void);
void vga_write(const char *s, size_t n);
void kbd_init(void);
size_t log_read(char *dst, size_t n);

/* Linux device numbers */
#define MKDEV(ma, mi) (((uint64_t)(ma) << 8) | ((mi) & 0xff) | ((uint64_t)((mi) & ~0xff) << 12))
#define MAJOR(d) ((unsigned)(((d) >> 8) & 0xfff))
#define MINOR(d) ((unsigned)(((d) & 0xff) | (((d) >> 12) & ~0xffU)))

struct file;
struct inode;
struct file_ops;

/* character devices: open() picks the file_ops for a minor */
struct chrdev {
  const char *name;
  int (*open)(struct inode *ino, struct file *f);
};
void chrdev_register(unsigned major, const struct chrdev *d);
int chrdev_open(struct inode *ino, struct file *f);

/* block devices, addressed in 512-byte sectors */
struct blockdev {
  char name[16];
  dev_t dev;
  uint64_t sectors;
  int (*read)(struct blockdev *b, uint64_t sector, unsigned count, void *buf);
  int (*write)(struct blockdev *b, uint64_t sector, unsigned count, const void *buf);
  int (*flush)(struct blockdev *b);  /* the device's write cache to the medium, or NULL */
  bool unflushed;                    /* written since the last flush */
  void *priv;
};
void blockdev_register(struct blockdev *b);
struct blockdev *blockdev_get(dev_t dev);
struct blockdev *blockdev_by_name(const char *name);
struct blockdev *blockdev_nth(int i);
void ata_init(void);
void virtio_blk_init(void);

/* buffer cache (fs/bcache.c) */
struct buf {
  struct blockdev *bdev;
  uint64_t block;
  unsigned size;
  int ref;
  bool dirty, valid;
  uint8_t *data;
  struct list lru, hash;
  struct list dirty_link;  /* on the list of dirty buffers while dirty */
};
struct buf *bread(struct blockdev *b, uint64_t block, unsigned size);
struct buf *bget(struct blockdev *b, uint64_t block, unsigned size); /* no read */
void bdirty(struct buf *bp);
void brelse(struct buf *bp);
void bsync(struct blockdev *b);        /* may let others run meanwhile */
void bsync_atomic(struct blockdev *b); /* does not */
void bforget(struct blockdev *b, uint64_t block, unsigned size);
void bcache_init(void);
uint64_t bcache_reclaim(uint64_t npages); /* pages freed */
uint64_t bcache_kb(void);

/* PCI (drivers/pci.c) */
struct pci_dev {
  uint8_t bus, dev, fn, irq;
  uint16_t vendor, device;
  uint32_t bar[6];
};
uint32_t pci_read(const struct pci_dev *d, int off);
void pci_write(const struct pci_dev *d, int off, uint32_t v);
bool pci_find(const uint16_t *ids, struct pci_dev *out);

/* typed memory objects (drivers/tym.c) */
void tym_init(const char *conf);
void tym_populate(void);

/* network cards */
bool e1000_init(void);
void e1000_poll(void); /* received frames to net_rx (the network thread) */

/* random numbers (drivers/mem.c) */
void get_random_bytes(void *buf, size_t n);
void random_mix(uint64_t v);
void mem_devices_init(void);

#endif
