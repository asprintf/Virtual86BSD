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

#endif
