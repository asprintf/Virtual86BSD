/* conf.h - console, character and block devices. */
#ifndef _SYS_CONF_H_
#define _SYS_CONF_H_

#include <sys/systm.h>

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
