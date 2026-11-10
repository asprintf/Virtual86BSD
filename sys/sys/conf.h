/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* conf.h - console, character and block devices. */
#ifndef _SYS_CONF_H_
#define _SYS_CONF_H_

#include <sys/systm.h>

/* configuring the machine: the devices (arch/amd64/amd64/autoconf.c) */
void cpu_configure(void);

/* console output: every console device (serial, VGA) */
void console_write(const char *s, size_t n);
void serial_init(void);
void serial_write(const char *s, size_t n);
void vga_init(void);
void vga_write(const char *s, size_t n);
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
#define NBLOCKDEV 64 /* disks and their partitions */
void blockdev_register(struct blockdev *b);
struct blockdev *blockdev_get(dev_t dev);
struct blockdev *blockdev_by_name(const char *name);
struct blockdev *blockdev_nth(int i);

/* typed memory objects (drivers/tym.c) */
void tym_init(const char *conf);
void tym_populate(void);
void tym_report(void);

/* network cards */
void em_start(void);   /* the card's interrupt on (the network is ready for it) */
void e1000_poll(void); /* received frames to net_rx (the network thread) */

/* random numbers (drivers/mem.c) */
void get_random_bytes(void *buf, size_t n);
void random_mix(uint64_t v);
void mem_devices_init(void);

#endif
