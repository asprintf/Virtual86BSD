/* disk.h - disks as devices (kern/subr_disk.c): what a controller driver
   (pciide, vioblk) offers as the aux of a device at it. */
#ifndef _SYS_DISK_H_
#define _SYS_DISK_H_

#include <sys/conf.h>

/* disk0, disk1 ...: the driver made the block device bd (not registered
   yet) for what it found; the disk registers it and says what it is.  A
   controller that tells more of where the disk is (pciide: channel and
   drive) puts this first in its own aux and has its own print function */
struct disk_attach_args {
  struct blockdev *da_bd;
  const char *da_desc; /* the drive's name: <QEMU HARDDISK> */
};

/* a partition table entry (kern/subr_disk.c): its number in the table, where
   it starts and how long it is, in sectors */
struct ptab_entry {
  int index; /* 1 ...: the name's number, disk0s<index> */
  uint64_t start, sectors;
  int type;  /* MBR: the type byte */
};

/* what a disk's table says (pure functions of the sectors, for the tests too) */
uint32_t crc32(const void *buf, size_t n);
int ptab_mbr(const uint8_t *s0, uint64_t disksectors, struct ptab_entry *e, bool *protective);
int ptab_gpt(const uint8_t *hdr, uint64_t disksectors, int (*get)(void *arg, uint64_t lba, unsigned n, void *buf),
             void *arg, struct ptab_entry *e, int maxe, const char **why);

/* the raw devices rdisk0, rdisk0s1 ...: a character major of their own */
#define RDISK_MAJOR 241
extern bool rdisk_present;

#endif
