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

/* mem.c - the character device table; /dev/null, zero, full, random,
   urandom, kmsg, klog; raw block devices; the nodes of devtmpfs. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/disk.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>

static const struct chrdev *chrdevs[256];

void chrdev_register(unsigned major, const struct chrdev *d) {
  if (major < 256)
    chrdevs[major] = d;
}

int chrdev_open(struct inode *ino, struct file *f) {
  unsigned ma = MAJOR(ino->rdev);
  if (ma >= 256 || !chrdevs[ma])
    return -ENXIO;
  return chrdevs[ma]->open(ino, f);
}

/* ---- random numbers ----
   ChaCha20 (RFC 8439's block function) with fast key erasure: of each block,
   the first half is the next key and the second half is what is handed out.
   The key starts from the processor's RDSEED or RDRAND when it has them (and
   they are mixed in again at every call), and random_mix() adds the timer
   and the events of the keyboard and the network, so that the numbers
   (TCP's sequence numbers, ports, salts) cannot be guessed from earlier ones. */

static uint32_t rkey[8] = {0x243f6a88, 0x85a308d3, 0x13198a2e, 0x03707344, 0xa4093822, 0x299f31d0, 0x082efa98, 0xec4e6c89};
static uint64_t rctr;
static unsigned rmix;
static int rseeded, rhave; /* rhave: 1 RDRAND, 2 RDSEED too */

void random_mix(uint64_t v) {
  uint64_t h = (v ^ rdtsc64()) * 0x9e3779b97f4a7c15UL;
  rkey[rmix & 7] ^= (uint32_t)(h >> 32);
  rkey[(rmix + 1) & 7] ^= (uint32_t)h;
  rmix += 2;
}

static void random_cpu(void) {
  uint64_t v;
  if (rhave >= 2 && rdrand64(&v, 1))
    random_mix(v), rkey[0] ^= (uint32_t)v;
  if (rhave >= 1 && rdrand64(&v, 0))
    random_mix(v), rkey[1] ^= (uint32_t)(v >> 32);
}

static inline uint32_t rotl32(uint32_t v, int n) { return v << n | v >> (32 - n); }

static void qr(uint32_t *x, int a, int b, int c, int d) {
  x[a] += x[b], x[d] = rotl32(x[d] ^ x[a], 16);
  x[c] += x[d], x[b] = rotl32(x[b] ^ x[c], 12);
  x[a] += x[b], x[d] = rotl32(x[d] ^ x[a], 8);
  x[c] += x[d], x[b] = rotl32(x[b] ^ x[c], 7);
}

static void chacha20_block(const uint32_t key[8], uint64_t ctr, uint8_t out[64]) {
  uint32_t in[16] = {0x61707865, 0x3320646e, 0x79622d32, 0x6b206574}, x[16];
  for (int i = 0; i < 8; i++)
    in[4 + i] = key[i];
  in[12] = (uint32_t)ctr, in[13] = (uint32_t)(ctr >> 32);
  for (int i = 0; i < 16; i++)
    x[i] = in[i];
  for (int r = 0; r < 10; r++) {
    qr(x, 0, 4, 8, 12), qr(x, 1, 5, 9, 13), qr(x, 2, 6, 10, 14), qr(x, 3, 7, 11, 15);
    qr(x, 0, 5, 10, 15), qr(x, 1, 6, 11, 12), qr(x, 2, 7, 8, 13), qr(x, 3, 4, 9, 14);
  }
  for (int i = 0; i < 16; i++) {
    uint32_t w = x[i] + in[i];
    out[4 * i] = (uint8_t)w, out[4 * i + 1] = (uint8_t)(w >> 8), out[4 * i + 2] = (uint8_t)(w >> 16),
    out[4 * i + 3] = (uint8_t)(w >> 24);
  }
}

static void random_seed(void) {
  uint32_t r[4];
  rseeded = 1;
  cpuid4(0, 0, r);
  uint32_t max = r[0];
  cpuid4(1, 0, r);
  if (r[2] & (1U << 30))
    rhave = 1;
  if (max >= 7) {
    cpuid4(7, 0, r);
    if (rhave && (r[1] & (1U << 18)))
      rhave = 2;
  }
  for (int i = 0; i < 4; i++)
    random_cpu();
}

void get_random_bytes(void *buf, size_t n) {
  uint8_t *p = buf, b[64];
  if (!rseeded)
    random_seed();
  random_mix(rdtsc64());
  random_cpu();
  while (n) {
    chacha20_block(rkey, rctr++, b);
    for (int i = 0; i < 8; i++) /* (the next key) */
      rkey[i] = (uint32_t)b[4 * i] | (uint32_t)b[4 * i + 1] << 8 | (uint32_t)b[4 * i + 2] << 16 | (uint32_t)b[4 * i + 3] << 24;
    size_t c = MIN(n, 32UL);
    memcpy(p, b + 32, c);
    p += c;
    n -= c;
  }
  for (volatile uint8_t *z = b; z < b + sizeof(b); z++)
    *z = 0;
}

long sys_getrandom(long ubuf, long n, long flags) {
  if (flags & ~7)
    return -EINVAL;
  if (n > 33554431)
    n = 33554431;
  uint8_t tmp[256];
  long done = 0;
  while (done < n) {
    size_t c = MIN((size_t)(n - done), sizeof(tmp));
    get_random_bytes(tmp, c);
    if (copy_to_user((char *)ubuf + done, tmp, c))
      return done ? done : -EFAULT;
    done += (long)c;
  }
  return done;
}

/* ---- memory devices (major 1) ---- */

static ssize_t null_read(struct file *f, char *buf, size_t n, off_t *pos) {
  (void)f, (void)buf, (void)n, (void)pos;
  return 0;
}
static ssize_t null_write(struct file *f, const char *buf, size_t n, off_t *pos) {
  (void)f, (void)buf, (void)pos;
  return (ssize_t)n;
}
static ssize_t zero_read(struct file *f, char *buf, size_t n, off_t *pos) {
  (void)f, (void)pos;
  return clear_user(buf, n) ? -EFAULT : (ssize_t)n;
}
static ssize_t full_write(struct file *f, const char *buf, size_t n, off_t *pos) {
  (void)f, (void)buf, (void)n, (void)pos;
  return -ENOSPC;
}
static ssize_t random_read(struct file *f, char *buf, size_t n, off_t *pos) {
  (void)f, (void)pos;
  return sys_getrandom((long)buf, (long)n, 0);
}
static ssize_t kmsg_read(struct file *f, char *buf, size_t n, off_t *pos) {
  (void)f;
  char *tmp = kmalloc(65536);
  size_t have = log_read(tmp, 65536);
  ssize_t r = 0;
  if ((uint64_t)*pos < have) {
    size_t c = MIN(n, have - (size_t)*pos);
    r = copy_to_user(buf, tmp + *pos, c) ? -EFAULT : (ssize_t)c;
    if (r > 0)
      *pos += r;
  }
  kfree(tmp);
  return r;
}
static ssize_t kmsg_write(struct file *f, const char *buf, size_t n, off_t *pos) {
  (void)f, (void)pos;
  char tmp[256];
  size_t c = MIN(n, sizeof(tmp) - 1);
  if (copy_from_user(tmp, buf, c))
    return -EFAULT;
  tmp[c] = 0;
  printf("%s", tmp);
  return (ssize_t)n;
}

/* /dev/klog: the kernel's messages for syslogd(8), each taken once; a
   read waits for a message (poll too), O_NONBLOCK does not.  What is
   copied is only taken after it reached the reader. */
#define KLOG_CHUNK 4096
static ssize_t klog_read(struct file *f, char *buf, size_t n, off_t *pos) {
  (void)pos;
  if (n == 0)
    return 0;
  uint64_t fl = irq_save();
  while (!klog_avail()) {
    if (f->flags & O_NONBLOCK) {
      irq_restore(fl);
      return -EAGAIN;
    }
    if (sleep_on(&klog_wq, 0, true) == -EINTR) {
      irq_restore(fl);
      return -ERESTARTSYS;
    }
  }
  irq_restore(fl);
  char *tmp = kmalloc(KLOG_CHUNK);
  if (!tmp)
    return -ENOMEM;
  size_t k = klog_peek(tmp, MIN(n, (size_t)KLOG_CHUNK));
  ssize_t r = copy_to_user(buf, tmp, k) ? -EFAULT : (ssize_t)k;
  kfree(tmp);
  if (r > 0)
    klog_consume((size_t)r);
  return r;
}
static unsigned klog_poll(struct file *f) {
  (void)f;
  return klog_avail() ? POLLIN | POLLRDNORM : 0;
}
static void klog_release(struct file *f) {
  (void)f;
  klog_close();
}

static const struct file_ops null_fops = {.read = null_read, .write = null_write, .seekable = true};
static const struct file_ops zero_fops = {.read = zero_read, .write = null_write, .seekable = true};
static const struct file_ops full_fops = {.read = zero_read, .write = full_write, .seekable = true};
static const struct file_ops random_fops = {.read = random_read, .write = null_write};
static const struct file_ops kmsg_fops = {.read = kmsg_read, .write = kmsg_write};
static const struct file_ops klog_fops = {.read = klog_read, .poll = klog_poll, .release = klog_release};

static int mem_open(struct inode *ino, struct file *f) {
  switch (MINOR(ino->rdev)) {
  case 3: f->fops = &null_fops; return 0;
  case 5: f->fops = &zero_fops; return 0;
  case 7: f->fops = &full_fops; return 0;
  case 8:
  case 9: f->fops = &random_fops; return 0;
  case 11: f->fops = &kmsg_fops; return 0;
  case 13:
    klog_open();
    f->fops = &klog_fops;
    return 0;
  }
  return -ENXIO;
}
static const struct chrdev memdev = {"mem", mem_open};

/* ---- block device files ---- */

static ssize_t blk_rw(struct file *f, char *buf, size_t n, off_t *pos, bool write) {
  struct blockdev *b = f->priv;
  uint64_t size = b->sectors * 512;
  if ((uint64_t)*pos >= size)
    return write && n ? -ENOSPC : 0;
  n = (size_t)MIN((uint64_t)n, size - (uint64_t)*pos);
  size_t done = 0;
  while (done < n) {
    uint64_t p = (uint64_t)*pos;
    struct buf *bp = bread(b, p / 4096, 4096);
    size_t off = p % 4096, c = MIN(n - done, 4096 - off);
    int r;
    if (write) {
      r = copy_from_user(bp->data + off, buf + done, c);
      bdirty(bp);
    } else
      r = copy_to_user(buf + done, bp->data + off, c);
    brelse(bp);
    if (r)
      return done ? (ssize_t)done : -EFAULT;
    done += c;
    *pos += (off_t)c;
  }
  return (ssize_t)done;
}
static ssize_t blk_read(struct file *f, char *buf, size_t n, off_t *pos) { return blk_rw(f, buf, n, pos, false); }
static ssize_t blk_write(struct file *f, const char *buf, size_t n, off_t *pos) {
  return blk_rw(f, (char *)buf, n, pos, true);
}
static int blk_fsync(struct file *f) {
  bsync(f->priv);
  return 0;
}
static const struct file_ops blk_fops = {.read = blk_read, .write = blk_write, .fsync = blk_fsync, .seekable = true};

/* generic open of device and socket inodes of any file system */
static int special_open(struct inode *ino, struct file *f) {
  if (S_ISCHR(ino->mode))
    return chrdev_open(ino, f);
  if (S_ISBLK(ino->mode)) {
    struct blockdev *b = blockdev_get(ino->rdev);
    if (!b)
      return -ENXIO;
    f->priv = b;
    f->fops = &blk_fops;
    return 0;
  }
  return -ENXIO; /* sockets are not opened */
}
const struct file_ops special_fops = {.open = special_open};

/* ---- devtmpfs contents ---- */

static struct super *devsb;

struct inode *devtmpfs_add(const char *name, mode_t mode, dev_t rdev) {
  if (!devsb)
    return NULL;
  return tmpfs_mknod_path(devsb, name, mode, rdev);
}

void devtmpfs_remove(const char *name) {
  if (!devsb)
    return;
  const char *slash = strrchr(name, '/');
  struct inode *dir = devsb->root;
  struct inode *held = NULL;
  if (slash) {
    dir = dir->iops->lookup(dir, name, (size_t)(slash - name));
    if (IS_ERR(dir))
      return;
    held = dir;
    name = slash + 1;
  }
  struct inode *i = dir->iops->lookup(dir, name, strlen(name));
  if (!IS_ERR(i)) {
    dir->iops->unlink(dir, name, strlen(name), i);
    iput(i);
  }
  if (held)
    iput(held);
}

int devtmpfs_populate(struct super *sb) {
  devsb = sb;
  static const struct {
    const char *name;
    mode_t mode;
    unsigned ma, mi;
  } nodes[] = {
      {"null", S_IFCHR | 0666, 1, 3},     {"zero", S_IFCHR | 0666, 1, 5},
      {"full", S_IFCHR | 0666, 1, 7},     {"random", S_IFCHR | 0666, 1, 8},
      {"urandom", S_IFCHR | 0666, 1, 9},  {"kmsg", S_IFCHR | 0644, 1, 11},
      {"klog", S_IFCHR | 0600, 1, 13},
      {"tty", S_IFCHR | 0666, 5, 0},      {"console", S_IFCHR | 0600, 5, 1},
      {"ptmx", S_IFCHR | 0666, 5, 2},     {"tty0", S_IFCHR | 0620, 4, 0},
      {"ttyC0", S_IFCHR | 0620, 4, 1},    {"tty00", S_IFCHR | 0660, 4, 64},
      {"cua00", S_IFCHR | 0660, 4, 64},
  };
  for (size_t i = 0; i < ARRAY_SIZE(nodes); i++)
    tmpfs_mknod_path(sb, nodes[i].name, nodes[i].mode, MKDEV(nodes[i].ma, nodes[i].mi));
  for (int i = 0; i < NBLOCKDEV; i++) {
    struct blockdev *b = blockdev_nth(i);
    if (!b)
      continue;
    tmpfs_mknod_path(sb, b->name, S_IFBLK | 0660, b->dev);
    if (rdisk_present) {
      char raw[24];
      snprintf(raw, sizeof(raw), "r%s", b->name);
      tmpfs_mknod_path(sb, raw, S_IFCHR | 0660, MKDEV(RDISK_MAJOR, MINOR(b->dev)));
    }
  }
  tmpfs_mknod_path(sb, "pts", S_IFDIR | 0755, 0);
  tym_populate();
  struct inode *shm = tmpfs_mknod_path(sb, "shm", S_IFDIR | 01777, 0);
  (void)shm;
  struct inode *root = sb->root;
  static const char *links[][2] = {
      {"fd", "/proc/self/fd"}, {"stdin", "/proc/self/fd/0"}, {"stdout", "/proc/self/fd/1"}, {"stderr", "/proc/self/fd/2"}};
  for (size_t i = 0; i < ARRAY_SIZE(links); i++) {
    struct inode *l = root->iops->symlink(root, links[i][0], strlen(links[i][0]), links[i][1]);
    if (!IS_ERR(l)) {
      l->uid = l->gid = 0;
      iput(l);
    }
  }
  return 0;
}

void mem_devices_init(void) { chrdev_register(1, &memdev); }
