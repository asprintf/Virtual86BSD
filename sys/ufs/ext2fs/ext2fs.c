/* ext2.c - the second extended file system, read and write.  Revision 0
   and 1, 1-64 KiB blocks, 128-byte or larger inodes, the "filetype"
   feature.  Unknown incompatible features refuse the mount; unknown
   read-only-compatible features make it read-only.  Hashed directory
   indexes (dir_index) are ignored, as Linux's ext2 does. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <uvm/uvm.h>

#define EXT2_MAGIC 0xef53
#define ROOT_INO 2
#define INCOMPAT_FILETYPE 0x2
#define RO_SPARSE 1
#define RO_LARGE 2
#define RO_BTREE 4
#define EXT2_INDEX_FL 0x1000

struct ext2_sb {
  struct super *sb;
  struct blockdev *b;
  unsigned bs;            /* block size */
  uint32_t blocks, inodes, first_data, bpg, ipg, ngroups, first_ino;
  unsigned isize;         /* inode size */
  uint16_t mount_state;   /* the state when mounted: restored when it stops */
  uint32_t incompat, rocompat;
  uint8_t raw[1024];      /* the superblock */
  uint8_t *gd;            /* group descriptors */
  unsigned gd_blocks;
  bool dirty;
};

struct ext2_ino {
  uint32_t block[15];
  uint32_t flags, file_acl, group;
};

static const struct inode_ops ext2_dir_iops, ext2_file_iops, ext2_link_iops, ext2_special_iops;
static const struct file_ops ext2_dir_fops, ext2_file_fops;
extern const struct file_ops fifo_fops;
static const struct super_ops ext2_sops;

static inline uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static inline uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static inline void wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24); }
static inline void wr16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }

static struct ext2_sb *E(struct super *sb) { return sb->priv; }
static struct ext2_ino *EI(struct inode *i) { return i->priv; }

static uint8_t *gdesc(struct ext2_sb *e, uint32_t g) { return e->gd + g * 32; }

static void sb_write(struct ext2_sb *e) {
  /* the superblock is at byte 1024 */
  unsigned blk = 1024 / e->bs, off = 1024 % e->bs;
  struct buf *bp = bread(e->b, blk, e->bs);
  memcpy(bp->data + off, e->raw, 1024);
  bdirty(bp);
  brelse(bp);
  for (unsigned i = 0; i < e->gd_blocks; i++) {
    struct buf *g = bget(e->b, e->first_data + 1 + i, e->bs);
    memcpy(g->data, e->gd + i * e->bs, e->bs);
    bdirty(g);
    brelse(g);
  }
  e->dirty = false;
}

/* the superblock and group descriptors into their buffers; sync_all()
   writes the buffers afterwards (bsync) */
static void ext2_sync(struct super *sb) {
  struct ext2_sb *e = E(sb);
  if (e->dirty && !sb->rdonly)
    sb_write(e);
}

/* power off or reboot: the file system stays mounted, but is consistent */
static void ext2_shutdown(struct super *sb) {
  struct ext2_sb *e = E(sb);
  if (sb->rdonly)
    return;
  wr16(e->raw + 58, e->mount_state); /* clean, unless it was not when mounted */
  sb_write(e);
  bsync_atomic(e->b);
}

static void ext2_umount(struct super *sb) {
  struct ext2_sb *e = E(sb);
  if (!sb->rdonly) {
    wr16(e->raw + 58, e->mount_state);
    sb_write(e);
  }
  bsync_atomic(e->b);
  kfree(e->gd);
  kfree(e);
  kfree(sb);
}

/* ---- allocation ---- */

static void adjust_counts(struct ext2_sb *e, uint32_t g, int dblocks, int dinodes, int ddirs) {
  uint8_t *d = gdesc(e, g);
  wr16(d + 12, (uint16_t)(rd16(d + 12) + dblocks));
  wr16(d + 14, (uint16_t)(rd16(d + 14) + dinodes));
  wr16(d + 16, (uint16_t)(rd16(d + 16) + ddirs));
  wr32(e->raw + 12, rd32(e->raw + 12) + (uint32_t)dblocks);
  wr32(e->raw + 16, rd32(e->raw + 16) + (uint32_t)dinodes);
  e->dirty = true;
}

static uint32_t alloc_block(struct ext2_sb *e, uint32_t goal_group) {
  for (uint32_t k = 0; k < e->ngroups; k++) {
    uint32_t g = (goal_group + k) % e->ngroups;
    uint8_t *d = gdesc(e, g);
    if (rd16(d + 12) == 0)
      continue;
    struct buf *bm = bread(e->b, rd32(d), e->bs);
    uint32_t n = MIN(e->bpg, e->blocks - e->first_data - g * e->bpg);
    for (uint32_t i = 0; i < n; i++) {
      if (bm->data[i / 8] == 0xff) {
        i |= 7;
        continue;
      }
      if (!(bm->data[i / 8] & (1 << (i % 8)))) {
        bm->data[i / 8] |= (uint8_t)(1 << (i % 8));
        bdirty(bm);
        brelse(bm);
        adjust_counts(e, g, -1, 0, 0);
        uint32_t blk = e->first_data + g * e->bpg + i;
        struct buf *z = bget(e->b, blk, e->bs);
        memset(z->data, 0, e->bs);
        bdirty(z);
        brelse(z);
        return blk;
      }
    }
    brelse(bm);
  }
  return 0;
}

static void free_block(struct ext2_sb *e, uint32_t blk) {
  if (blk < e->first_data || blk >= e->blocks)
    return;
  uint32_t g = (blk - e->first_data) / e->bpg, i = (blk - e->first_data) % e->bpg;
  struct buf *bm = bread(e->b, rd32(gdesc(e, g)), e->bs);
  if (bm->data[i / 8] & (1 << (i % 8))) {
    bm->data[i / 8] &= (uint8_t)~(1 << (i % 8));
    bdirty(bm);
    adjust_counts(e, g, 1, 0, 0);
  }
  brelse(bm);
  bforget(e->b, blk, e->bs); /* its data need not be written any more */
}

static uint32_t alloc_inode(struct ext2_sb *e, uint32_t goal_group, bool dir) {
  for (uint32_t k = 0; k < e->ngroups; k++) {
    uint32_t g = (goal_group + k) % e->ngroups;
    uint8_t *d = gdesc(e, g);
    if (rd16(d + 14) == 0)
      continue;
    struct buf *bm = bread(e->b, rd32(d + 4), e->bs);
    for (uint32_t i = 0; i < e->ipg; i++) {
      uint32_t ino = g * e->ipg + i + 1;
      if (ino < e->first_ino)
        continue;
      if (!(bm->data[i / 8] & (1 << (i % 8)))) {
        bm->data[i / 8] |= (uint8_t)(1 << (i % 8));
        bdirty(bm);
        brelse(bm);
        adjust_counts(e, g, 0, -1, dir ? 1 : 0);
        return ino;
      }
    }
    brelse(bm);
  }
  return 0;
}

static void free_inode_bit(struct ext2_sb *e, uint32_t ino, bool dir) {
  uint32_t g = (ino - 1) / e->ipg, i = (ino - 1) % e->ipg;
  struct buf *bm = bread(e->b, rd32(gdesc(e, g) + 4), e->bs);
  if (bm->data[i / 8] & (1 << (i % 8))) {
    bm->data[i / 8] &= (uint8_t)~(1 << (i % 8));
    bdirty(bm);
    adjust_counts(e, g, 0, 1, dir ? -1 : 0);
  }
  brelse(bm);
}

/* ---- inodes ---- */

static struct buf *inode_buf(struct ext2_sb *e, ino_t ino, unsigned *off) {
  uint32_t g = (uint32_t)(ino - 1) / e->ipg, idx = (uint32_t)(ino - 1) % e->ipg;
  uint64_t byte = (uint64_t)idx * e->isize;
  uint32_t blk = rd32(gdesc(e, g) + 8) + (uint32_t)(byte / e->bs);
  *off = (unsigned)(byte % e->bs);
  return bread(e->b, blk, e->bs);
}

static void set_ops(struct inode *i) {
  switch (i->mode & S_IFMT) {
  case S_IFDIR:
    i->iops = &ext2_dir_iops;
    i->fops = &ext2_dir_fops;
    break;
  case S_IFREG:
    i->iops = &ext2_file_iops;
    i->fops = &ext2_file_fops;
    break;
  case S_IFLNK:
    i->iops = &ext2_link_iops;
    i->fops = NULL;
    break;
  case S_IFIFO:
    i->iops = &ext2_special_iops;
    i->fops = &fifo_fops;
    break;
  default:
    i->iops = &ext2_special_iops;
    i->fops = NULL; /* devices: chrdev_open via the generic special fops */
    break;
  }
}

extern const struct file_ops special_fops;

static int ext2_read_inode(struct inode *i) {
  struct ext2_sb *e = E(i->sb);
  if (i->ino < 1 || i->ino > e->inodes)
    return -EIO;
  unsigned off;
  struct buf *bp = inode_buf(e, i->ino, &off);
  const uint8_t *r = bp->data + off;
  struct ext2_ino *ei = kzalloc(sizeof(*ei));
  if (!ei) {
    brelse(bp);
    return -ENOMEM;
  }
  i->priv = ei;
  i->mode = rd16(r);
  i->uid = rd16(r + 2) | (uint32_t)rd16(r + 120) << 16;
  i->gid = rd16(r + 24) | (uint32_t)rd16(r + 122) << 16;
  i->size = rd32(r + 4);
  if (S_ISREG(i->mode))
    i->size |= (uint64_t)rd32(r + 108) << 32;
  i->atime.sec = (int32_t)rd32(r + 8);
  i->ctime.sec = (int32_t)rd32(r + 12);
  i->mtime.sec = (int32_t)rd32(r + 16);
  i->nlink = rd16(r + 26);
  i->blocks = rd32(r + 28);
  ei->flags = rd32(r + 32);
  for (int k = 0; k < 15; k++)
    ei->block[k] = rd32(r + 40 + 4 * k);
  ei->file_acl = rd32(r + 104);
  ei->group = (uint32_t)(i->ino - 1) / e->ipg;
  if (e->isize > 128 && rd16(r + 128) >= 16) {
    /* as ext4: seconds = signed 32 bits + epoch << 32 */
    uint32_t c = rd32(r + 132), m = rd32(r + 136), a = rd32(r + 140);
    i->ctime.nsec = c >> 2, i->ctime.sec += (int64_t)(c & 3) << 32;
    i->mtime.nsec = m >> 2, i->mtime.sec += (int64_t)(m & 3) << 32;
    i->atime.nsec = a >> 2, i->atime.sec += (int64_t)(a & 3) << 32;
    i->ctime.nsec %= 1000000000, i->mtime.nsec %= 1000000000, i->atime.nsec %= 1000000000;
  }
  if (S_ISCHR(i->mode) || S_ISBLK(i->mode)) {
    uint32_t old = ei->block[0], nw = ei->block[1];
    if (old)
      i->rdev = MKDEV((old >> 8) & 0xff, old & 0xff);
    else
      i->rdev = MKDEV((nw & 0xfff00) >> 8, (nw & 0xff) | ((nw >> 12) & 0xfff00));
  }
  brelse(bp);
  set_ops(i);
  if (S_ISCHR(i->mode) || S_ISBLK(i->mode) || S_ISSOCK(i->mode))
    i->fops = &special_fops;
  if (i->mode == 0 && i->nlink == 0)
    return -ESTALE;
  return 0;
}

/* the epoch bits of an extended timestamp: what the signed 32-bit field
   leaves out, in units of 2^32 seconds */
static uint32_t epoch_bits(int64_t sec) { return (uint32_t)(((sec - (int64_t)(int32_t)sec) >> 32) & 3); }

static int ext2_write_inode(struct inode *i) {
  struct ext2_sb *e = E(i->sb);
  if (i->sb->rdonly)
    return 0;
  struct ext2_ino *ei = EI(i);
  unsigned off;
  struct buf *bp = inode_buf(e, i->ino, &off);
  uint8_t *r = bp->data + off;
  wr16(r, (uint16_t)i->mode);
  wr16(r + 2, (uint16_t)i->uid);
  wr16(r + 120, (uint16_t)(i->uid >> 16));
  wr16(r + 24, (uint16_t)i->gid);
  wr16(r + 122, (uint16_t)(i->gid >> 16));
  wr32(r + 4, (uint32_t)i->size);
  if (S_ISREG(i->mode)) {
    wr32(r + 108, (uint32_t)(i->size >> 32));
    if (i->size >> 31 && !(e->rocompat & RO_LARGE)) {
      e->rocompat |= RO_LARGE;
      wr32(e->raw + 100, e->rocompat);
      e->dirty = true;
    }
  }
  wr32(r + 8, (uint32_t)i->atime.sec);
  wr32(r + 12, (uint32_t)i->ctime.sec);
  wr32(r + 16, (uint32_t)i->mtime.sec);
  wr16(r + 26, (uint16_t)i->nlink);
  wr32(r + 28, (uint32_t)i->blocks);
  wr32(r + 32, ei->flags);
  if (S_ISCHR(i->mode) || S_ISBLK(i->mode)) {
    unsigned ma = MAJOR(i->rdev), mi = MINOR(i->rdev);
    if (ma < 256 && mi < 256) {
      ei->block[0] = ma << 8 | mi;
      ei->block[1] = 0;
    } else {
      ei->block[0] = 0;
      ei->block[1] = (mi & 0xff) | (ma << 8) | ((mi & ~0xffU) << 12);
    }
  }
  for (int k = 0; k < 15; k++)
    wr32(r + 40 + 4 * k, ei->block[k]);
  wr32(r + 104, ei->file_acl);
  if (e->isize > 128 && rd16(r + 128) >= 16) {
    wr32(r + 132, (uint32_t)i->ctime.nsec << 2 | epoch_bits(i->ctime.sec));
    wr32(r + 136, (uint32_t)i->mtime.nsec << 2 | epoch_bits(i->mtime.sec));
    wr32(r + 140, (uint32_t)i->atime.nsec << 2 | epoch_bits(i->atime.sec));
  }
  bdirty(bp);
  brelse(bp);
  i->dirty = false;
  return 0;
}

static void ext2_put(struct inode *i) { kfree(i->priv); }

/* ---- block mapping ---- */

/* the physical block of logical block lb; create allocates it (and the
   indirect blocks on the way) */
static uint32_t bmap(struct inode *i, uint64_t lb, bool create, int *err) {
  struct ext2_sb *e = E(i->sb);
  struct ext2_ino *ei = EI(i);
  uint32_t per = e->bs / 4;
  *err = 0;
  if (lb < 12) {
    if (!ei->block[lb] && create) {
      ei->block[lb] = alloc_block(e, ei->group);
      if (!ei->block[lb]) {
        *err = -ENOSPC;
        return 0;
      }
      i->blocks += e->bs / 512;
      i->dirty = true;
    }
    return ei->block[lb];
  }
  lb -= 12;
  int levels;
  uint32_t *root;
  if (lb < per) {
    levels = 1;
    root = &ei->block[12];
  } else if ((lb -= per) < (uint64_t)per * per) {
    levels = 2;
    root = &ei->block[13];
  } else if ((lb -= (uint64_t)per * per) < (uint64_t)per * per * per) {
    levels = 3;
    root = &ei->block[14];
  } else {
    *err = -EFBIG;
    return 0;
  }
  if (!*root) {
    if (!create)
      return 0;
    *root = alloc_block(e, ei->group);
    if (!*root) {
      *err = -ENOSPC;
      return 0;
    }
    i->blocks += e->bs / 512;
    i->dirty = true;
  }
  uint32_t blk = *root;
  for (int l = levels - 1; l >= 0; l--) {
    uint64_t span = 1;
    for (int k = 0; k < l; k++)
      span *= per;
    uint32_t idx = (uint32_t)(lb / span);
    lb %= span;
    struct buf *bp = bread(e->b, blk, e->bs);
    uint32_t next = rd32(bp->data + 4 * idx);
    if (!next && create) {
      next = alloc_block(e, ei->group);
      if (!next) {
        brelse(bp);
        *err = -ENOSPC;
        return 0;
      }
      wr32(bp->data + 4 * idx, next);
      bdirty(bp);
      i->blocks += e->bs / 512;
      i->dirty = true;
    }
    brelse(bp);
    if (!next)
      return 0;
    blk = next;
  }
  return blk;
}

/* free everything in an indirect tree from logical index `from` on;
   returns true if the whole block became empty (and was freed) */
static bool free_tree(struct inode *i, uint32_t blk, int level, uint64_t from) {
  struct ext2_sb *e = E(i->sb);
  uint32_t per = e->bs / 4;
  uint64_t span = 1;
  for (int k = 0; k < level - 1; k++)
    span *= per;
  struct buf *bp = bread(e->b, blk, e->bs);
  bool empty = true;
  for (uint32_t k = 0; k < per; k++) {
    uint32_t c = rd32(bp->data + 4 * k);
    if (!c)
      continue;
    uint64_t lo = k * span;
    if (lo + span <= from) {
      empty = false;
      continue;
    }
    bool gone;
    if (level == 1) {
      free_block(e, c);
      i->blocks -= e->bs / 512;
      gone = true;
    } else
      gone = free_tree(i, c, level - 1, from > lo ? from - lo : 0);
    if (gone) {
      wr32(bp->data + 4 * k, 0);
      bdirty(bp);
    } else
      empty = false;
  }
  brelse(bp);
  if (empty) {
    free_block(e, blk);
    i->blocks -= e->bs / 512;
  }
  return empty;
}

static void free_blocks_from(struct inode *i, uint64_t from) {
  struct ext2_sb *e = E(i->sb);
  struct ext2_ino *ei = EI(i);
  uint32_t per = e->bs / 4;
  for (uint64_t k = from; k < 12; k++)
    if (ei->block[k]) {
      free_block(e, ei->block[k]);
      ei->block[k] = 0;
      i->blocks -= e->bs / 512;
    }
  uint64_t base = 12, span = per;
  for (int l = 1; l <= 3; l++) {
    uint32_t *root = &ei->block[11 + l];
    if (*root && from < base + span) {
      if (free_tree(i, *root, l, from > base ? from - base : 0))
        *root = 0;
    }
    base += span;
    span *= per;
  }
  i->dirty = true;
}

static bool fast_symlink(struct inode *i) {
  struct ext2_sb *e = E(i->sb);
  uint64_t acl = EI(i)->file_acl ? e->bs / 512 : 0;
  return S_ISLNK(i->mode) && i->blocks == acl;
}

static int ext2_truncate(struct inode *i, uint64_t size) {
  struct ext2_sb *e = E(i->sb);
  if (i->sb->rdonly)
    return -EROFS;
  if (S_ISLNK(i->mode) && fast_symlink(i)) {
    i->size = size;
    return 0;
  }
  if (size < i->size) {
    uint64_t keep = (size + e->bs - 1) / e->bs;
    free_blocks_from(i, keep);
    /* zero the tail of the last kept block */
    if (size % e->bs) {
      int err;
      uint32_t b = bmap(i, size / e->bs, false, &err);
      if (b) {
        struct buf *bp = bread(e->b, b, e->bs);
        memset(bp->data + size % e->bs, 0, e->bs - size % e->bs);
        bdirty(bp);
        brelse(bp);
      }
    }
  }
  i->size = size;
  i->dirty = true;
  pcache_resize(i);
  return 0;
}

/* ---- file I/O ----
   Where the page cache (fs/pcache.c: shared mappings) holds a page, that
   page has the file's current contents: read() takes them from it, write()
   updates it as well as the blocks.  (Blocks are at most a page.) */

/* the page cache's access to the blocks */
static int ext2_rawio(struct inode *i, void *buf, size_t n, uint64_t pos, bool write) {
  struct ext2_sb *e = E(i->sb);
  char *k = buf;
  if (write && i->sb->rdonly)
    return -EROFS;
  while (n) {
    unsigned off = (unsigned)(pos % e->bs);
    size_t chunk = MIN(n, e->bs - off);
    int err;
    uint32_t b = bmap(i, pos / e->bs, write, &err);
    if (!b) {
      if (write)
        return err ? err : -ENOSPC;
      memset(k, 0, chunk);
    } else {
      struct buf *bp = (write && off == 0 && chunk == e->bs) ? bget(e->b, b, e->bs) : bread(e->b, b, e->bs);
      if (write) {
        memcpy(bp->data + off, k, chunk);
        bdirty(bp);
      } else
        memcpy(k, bp->data + off, chunk);
      brelse(bp);
    }
    k += chunk, pos += chunk, n -= chunk;
  }
  return 0;
}

static ssize_t ext2_read(struct file *f, char *buf, size_t n, off_t *pos) {
  struct inode *i = f->ino;
  struct ext2_sb *e = E(i->sb);
  if (*pos < 0)
    return -EINVAL;
  if ((uint64_t)*pos >= i->size)
    return 0;
  if (n > i->size - (uint64_t)*pos)
    n = (size_t)(i->size - (uint64_t)*pos);
  size_t done = 0;
  while (done < n) {
    uint64_t p = (uint64_t)*pos;
    unsigned off = (unsigned)(p % e->bs);
    size_t chunk = MIN(n - done, e->bs - off);
    int err;
    char *cp = pcache_lookup(i, p);
    uint32_t b = cp ? 0 : bmap(i, p / e->bs, false, &err);
    int r;
    if (cp)
      r = copy_to_user(buf + done, cp + (p & (PAGE_SIZE - 1)), chunk);
    else if (!b)
      r = clear_user(buf + done, chunk);
    else {
      struct buf *bp = bread(e->b, b, e->bs);
      r = copy_to_user(buf + done, bp->data + off, chunk);
      brelse(bp);
    }
    if (r)
      return done ? (ssize_t)done : -EFAULT;
    done += chunk;
    *pos += (off_t)chunk;
  }
  if (!(f->flags & O_NOATIME) && !i->sb->rdonly) {
    /* relatime-free: POSIX atime */
    uint64_t ns = realtime_ns();
    i->atime.sec = (int64_t)(ns / 1000000000UL);
    i->atime.nsec = (int64_t)(ns % 1000000000UL);
    i->dirty = true;
  }
  return (ssize_t)done;
}

static ssize_t ext2_write(struct file *f, const char *buf, size_t n, off_t *pos) {
  struct inode *i = f->ino;
  struct ext2_sb *e = E(i->sb);
  if (i->sb->rdonly)
    return -EROFS;
  if (f->flags & O_APPEND)
    *pos = (off_t)i->size;
  if (*pos < 0)
    return -EINVAL;
  uint64_t lim = cur_proc()->rlim[RLIMIT_FSIZE].cur;
  if ((uint64_t)*pos >= lim && n) {
    signal_simple(cur_proc(), SIGXFSZ);
    return -EFBIG;
  }
  if ((uint64_t)*pos + n > lim)
    n = (size_t)(lim - (uint64_t)*pos);
  size_t done = 0;
  int err = 0;
  while (done < n) {
    uint64_t p = (uint64_t)*pos;
    unsigned off = (unsigned)(p % e->bs);
    size_t chunk = MIN(n - done, e->bs - off);
    uint32_t b = bmap(i, p / e->bs, true, &err);
    if (!b)
      break;
    struct buf *bp = (off == 0 && chunk == e->bs) ? bget(e->b, b, e->bs) : bread(e->b, b, e->bs);
    int r = copy_from_user(bp->data + off, buf + done, chunk);
    bdirty(bp);
    if (!r) {
      if ((uint64_t)*pos + chunk > i->size) {
        i->size = (uint64_t)*pos + chunk;
        pcache_resize(i);
      }
      char *cp = pcache_lookup(i, p);
      if (cp)
        memcpy(cp + (p & (PAGE_SIZE - 1)), bp->data + off, chunk);
    }
    brelse(bp);
    if (r) {
      err = -EFAULT;
      break;
    }
    done += chunk;
    *pos += (off_t)chunk;
  }
  if (done) {
    inode_set_times(i, false, true, true);
    /* writing clears set-user-ID (not for root) */
    if ((i->mode & (S_ISUID | S_ISGID)) && cur_proc()->cred.fsuid != 0) {
      i->mode &= ~S_ISUID;
      if (i->mode & 010)
        i->mode &= ~S_ISGID;
    }
    if (f->flags & O_SYNC)
      ext2_write_inode(i), bsync(e->b);
    return (ssize_t)done;
  }
  return err ? err : 0;
}

static int ext2_fsync(struct file *f) {
  struct inode *i = f->ino;
  int r = pcache_sync(i);
  if (r)
    return r;
  if (i->dirty)
    ext2_write_inode(i);
  ext2_sync(i->sb);
  bsync(E(i->sb)->b);
  return 0;
}

/* ---- directories ---- */

struct dirent_raw {
  uint32_t ino;
  uint16_t rec_len;
  uint8_t name_len, type;
};

static unsigned dtype_of(mode_t m) {
  switch (m & S_IFMT) {
  case S_IFREG: return 1;
  case S_IFDIR: return 2;
  case S_IFCHR: return 3;
  case S_IFBLK: return 4;
  case S_IFIFO: return 5;
  case S_IFSOCK: return 6;
  case S_IFLNK: return 7;
  }
  return 0;
}

static const unsigned ft_to_dt[8] = {DT_UNKNOWN, DT_REG, DT_DIR, DT_CHR, DT_BLK, DT_FIFO, DT_SOCK, DT_LNK};

/* walk the entries: fn returns true to stop; gives the buffer and offset */
typedef bool (*dir_fn)(struct buf *bp, unsigned off, uint64_t pos, void *arg);

static int dir_walk(struct inode *dir, uint64_t start, dir_fn fn, void *arg) {
  struct ext2_sb *e = E(dir->sb);
  for (uint64_t pos = start - start % e->bs; pos < dir->size; pos += e->bs) {
    int err;
    uint32_t b = bmap(dir, pos / e->bs, false, &err);
    if (!b)
      continue;
    struct buf *bp = bread(e->b, b, e->bs);
    unsigned off = 0;
    while (off + 8 <= e->bs) {
      uint8_t *d = bp->data + off;
      unsigned rl = rd16(d + 4);
      if (rl < 8 || off + rl > e->bs || (rl & 3))
        break; /* corrupt */
      if (pos + off >= start && fn(bp, off, pos + off, arg)) {
        brelse(bp);
        return 1;
      }
      off += rl;
    }
    brelse(bp);
  }
  return 0;
}

struct find_arg {
  const char *name;
  size_t len;
  uint32_t ino;
  uint64_t pos;
};

static bool find_fn(struct buf *bp, unsigned off, uint64_t pos, void *arg) {
  struct find_arg *a = arg;
  uint8_t *d = bp->data + off;
  if (rd32(d) && d[6] == a->len && !memcmp(d + 8, a->name, a->len)) {
    a->ino = rd32(d);
    a->pos = pos;
    return true;
  }
  return false;
}

static struct inode *ext2_lookup(struct inode *dir, const char *name, size_t len) {
  if (len > 255)
    return ERR_PTR(-ENAMETOOLONG);
  struct find_arg a = {name, len, 0, 0};
  if (!dir_walk(dir, 0, find_fn, &a))
    return ERR_PTR(-ENOENT);
  return iget(dir->sb, a.ino);
}

struct readdir_arg {
  struct dir_ctx *ctx;
  struct file *f;
  bool stop;
};

static bool readdir_fn(struct buf *bp, unsigned off, uint64_t pos, void *arg) {
  struct readdir_arg *a = arg;
  uint8_t *d = bp->data + off;
  uint64_t next = pos + rd16(d + 4);
  if (rd32(d)) {
    unsigned t = d[7] < 8 ? ft_to_dt[d[7]] : DT_UNKNOWN;
    if (!a->ctx->fill(a->ctx, (const char *)d + 8, d[6], rd32(d), t, (off_t)next)) {
      a->stop = true;
      return true;
    }
  }
  a->f->pos = (off_t)next;
  return false;
}

static int ext2_readdir(struct file *f, struct dir_ctx *ctx) {
  struct readdir_arg a = {ctx, f, false};
  dir_walk(f->ino, (uint64_t)f->pos, readdir_fn, &a);
  return 0;
}

static unsigned rec_size(size_t len) { return (unsigned)((8 + len + 3) & ~3UL); }

static int dir_add(struct inode *dir, const char *name, size_t len, uint32_t ino, mode_t mode) {
  struct ext2_sb *e = E(dir->sb);
  unsigned need = rec_size(len);
  uint8_t ft = (e->incompat & INCOMPAT_FILETYPE) ? (uint8_t)dtype_of(mode) : 0;
  for (uint64_t pos = 0; pos < dir->size; pos += e->bs) {
    int err;
    uint32_t b = bmap(dir, pos / e->bs, false, &err);
    if (!b)
      continue;
    struct buf *bp = bread(e->b, b, e->bs);
    unsigned off = 0;
    while (off + 8 <= e->bs) {
      uint8_t *d = bp->data + off;
      unsigned rl = rd16(d + 4);
      if (rl < 8)
        break;
      unsigned used = rd32(d) ? rec_size(d[6]) : 0;
      if (rl - used >= need) {
        uint8_t *n = d;
        if (used) {
          wr16(d + 4, (uint16_t)used);
          n = d + used;
          rl -= used;
        }
        wr32(n, ino);
        wr16(n + 4, (uint16_t)rl);
        n[6] = (uint8_t)len;
        n[7] = ft;
        memcpy(n + 8, name, len);
        bdirty(bp);
        brelse(bp);
        inode_set_times(dir, false, true, true);
        return 0;
      }
      off += rl;
    }
    brelse(bp);
  }
  /* a new block */
  int err;
  uint32_t b = bmap(dir, dir->size / e->bs, true, &err);
  if (!b)
    return err ? err : -ENOSPC;
  struct buf *bp = bread(e->b, b, e->bs);
  memset(bp->data, 0, e->bs);
  wr32(bp->data, ino);
  wr16(bp->data + 4, (uint16_t)e->bs);
  bp->data[6] = (uint8_t)len;
  bp->data[7] = ft;
  memcpy(bp->data + 8, name, len);
  bdirty(bp);
  brelse(bp);
  dir->size += e->bs;
  EI(dir)->flags &= ~EXT2_INDEX_FL;
  inode_set_times(dir, false, true, true);
  return 0;
}

static int dir_remove(struct inode *dir, const char *name, size_t len) {
  struct ext2_sb *e = E(dir->sb);
  for (uint64_t pos = 0; pos < dir->size; pos += e->bs) {
    int err;
    uint32_t b = bmap(dir, pos / e->bs, false, &err);
    if (!b)
      continue;
    struct buf *bp = bread(e->b, b, e->bs);
    unsigned off = 0, prev = ~0U;
    while (off + 8 <= e->bs) {
      uint8_t *d = bp->data + off;
      unsigned rl = rd16(d + 4);
      if (rl < 8)
        break;
      if (rd32(d) && d[6] == len && !memcmp(d + 8, name, len)) {
        if (prev != ~0U) {
          uint8_t *p = bp->data + prev;
          wr16(p + 4, (uint16_t)(rd16(p + 4) + rl));
        } else
          wr32(d, 0);
        bdirty(bp);
        brelse(bp);
        EI(dir)->flags &= ~EXT2_INDEX_FL;
        inode_set_times(dir, false, true, true);
        return 0;
      }
      prev = off;
      off += rl;
    }
    brelse(bp);
  }
  return -ENOENT;
}

/* change the inode an entry points to (rename over, "..") */
static int dir_set(struct inode *dir, const char *name, size_t len, uint32_t ino, mode_t mode) {
  struct ext2_sb *e = E(dir->sb);
  struct find_arg a = {name, len, 0, 0};
  if (!dir_walk(dir, 0, find_fn, &a))
    return -ENOENT;
  int err;
  uint32_t b = bmap(dir, a.pos / e->bs, false, &err);
  struct buf *bp = bread(e->b, b, e->bs);
  uint8_t *d = bp->data + a.pos % e->bs;
  wr32(d, ino);
  if (e->incompat & INCOMPAT_FILETYPE)
    d[7] = (uint8_t)dtype_of(mode);
  bdirty(bp);
  brelse(bp);
  inode_set_times(dir, false, true, true);
  return 0;
}

static bool empty_fn(struct buf *bp, unsigned off, uint64_t pos, void *arg) {
  (void)pos;
  uint8_t *d = bp->data + off;
  if (!rd32(d))
    return false;
  if ((d[6] == 1 && d[8] == '.') || (d[6] == 2 && d[8] == '.' && d[9] == '.'))
    return false;
  *(bool *)arg = false;
  return true;
}

static bool dir_empty(struct inode *dir) {
  bool empty = true;
  dir_walk(dir, 0, empty_fn, &empty);
  return empty;
}

/* ---- namespace operations ---- */

static struct inode *new_ext2_inode(struct inode *dir, mode_t mode) {
  struct ext2_sb *e = E(dir->sb);
  if (dir->sb->rdonly)
    return ERR_PTR(-EROFS);
  uint32_t ino = alloc_inode(e, EI(dir)->group, S_ISDIR(mode));
  if (!ino)
    return ERR_PTR(-ENOSPC);
  struct inode *i = new_inode(dir->sb, ino);
  if (!i) {
    free_inode_bit(e, ino, S_ISDIR(mode));
    return ERR_PTR(-ENOMEM);
  }
  struct ext2_ino *ei = kzalloc(sizeof(*ei));
  i->priv = ei;
  ei->group = (ino - 1) / e->ipg;
  i->mode = mode;
  struct cred *c = &cur_proc()->cred;
  i->uid = c->fsuid;
  i->gid = (dir->mode & S_ISGID) ? dir->gid : c->fsgid;
  if ((dir->mode & S_ISGID) && S_ISDIR(mode))
    i->mode |= S_ISGID;
  i->nlink = 1;
  /* clear the on-disk inode, keeping a large inode's extra size */
  unsigned off;
  struct buf *bp = inode_buf(e, ino, &off);
  memset(bp->data + off, 0, e->isize);
  if (e->isize > 128)
    wr16(bp->data + off + 128, (uint16_t)MIN(e->isize - 128, 32U));
  bdirty(bp);
  brelse(bp);
  inode_set_times(i, true, true, true);
  set_ops(i);
  if (S_ISCHR(mode) || S_ISBLK(mode) || S_ISSOCK(mode))
    i->fops = &special_fops;
  return i;
}

static struct inode *ext2_create(struct inode *dir, const char *name, size_t len, mode_t mode, dev_t rdev) {
  if (len > 255)
    return ERR_PTR(-ENAMETOOLONG);
  if (S_ISDIR(mode) && dir->nlink >= 65000)
    return ERR_PTR(-EMLINK);
  struct inode *i = new_ext2_inode(dir, mode);
  if (IS_ERR(i))
    return i;
  i->rdev = rdev;
  if (S_ISDIR(mode)) {
    struct ext2_sb *e = E(dir->sb);
    int err;
    uint32_t b = bmap(i, 0, true, &err);
    if (!b) {
      i->nlink = 0;
      iput(i);
      return ERR_PTR(err ? err : -ENOSPC);
    }
    struct buf *bp = bread(e->b, b, e->bs);
    memset(bp->data, 0, e->bs);
    uint8_t ft = (e->incompat & INCOMPAT_FILETYPE) ? 2 : 0;
    wr32(bp->data, (uint32_t)i->ino);
    wr16(bp->data + 4, 12);
    bp->data[6] = 1;
    bp->data[7] = ft;
    bp->data[8] = '.';
    wr32(bp->data + 12, (uint32_t)dir->ino);
    wr16(bp->data + 16, (uint16_t)(e->bs - 12));
    bp->data[18] = 2;
    bp->data[19] = ft;
    bp->data[20] = '.';
    bp->data[21] = '.';
    bdirty(bp);
    brelse(bp);
    i->size = e->bs;
    i->nlink = 2;
    dir->nlink++;
  }
  int r = dir_add(dir, name, len, (uint32_t)i->ino, mode);
  if (r) {
    if (S_ISDIR(mode))
      dir->nlink--;
    i->nlink = 0;
    iput(i);
    return ERR_PTR(r);
  }
  ext2_write_inode(i);
  ext2_write_inode(dir);
  return i;
}

static int ext2_link(struct inode *dir, const char *name, size_t len, struct inode *i) {
  if (dir->sb->rdonly)
    return -EROFS;
  if (len > 255)
    return -ENAMETOOLONG;
  int r = dir_add(dir, name, len, (uint32_t)i->ino, i->mode);
  if (r)
    return r;
  i->nlink++;
  i->dirty = true;
  return 0;
}

static int ext2_unlink(struct inode *dir, const char *name, size_t len, struct inode *i) {
  if (dir->sb->rdonly)
    return -EROFS;
  int r = dir_remove(dir, name, len);
  if (r)
    return r;
  i->nlink--;
  inode_set_times(i, false, false, true);
  return 0;
}

static int ext2_rmdir(struct inode *dir, const char *name, size_t len, struct inode *i) {
  if (dir->sb->rdonly)
    return -EROFS;
  if (!dir_empty(i))
    return -ENOTEMPTY;
  int r = dir_remove(dir, name, len);
  if (r)
    return r;
  i->nlink = 0;
  dir->nlink--;
  i->dirty = true;
  inode_set_times(dir, false, true, true);
  return 0;
}

static int ext2_rename(struct inode *odir, const char *oname, size_t olen, struct inode *i,
                       struct inode *ndir, const char *nname, size_t nlen, struct inode *target) {
  if (odir->sb->rdonly)
    return -EROFS;
  if (nlen > 255)
    return -ENAMETOOLONG;
  bool isdir = S_ISDIR(i->mode);
  if (target) {
    if (isdir && !dir_empty(target))
      return -ENOTEMPTY;
    int r = dir_set(ndir, nname, nlen, (uint32_t)i->ino, i->mode);
    if (r)
      return r;
    if (isdir) {
      target->nlink = 0;
      ndir->nlink--;
    } else
      target->nlink--;
    inode_set_times(target, false, false, true);
  } else {
    if (isdir && ndir != odir && ndir->nlink >= 65000)
      return -EMLINK;
    int r = dir_add(ndir, nname, nlen, (uint32_t)i->ino, i->mode);
    if (r)
      return r;
  }
  dir_remove(odir, oname, olen);
  if (isdir && odir != ndir) {
    dir_set(i, "..", 2, (uint32_t)ndir->ino, S_IFDIR);
    odir->nlink--;
    ndir->nlink++;
  }
  inode_set_times(i, false, false, true);
  ext2_write_inode(odir);
  if (ndir != odir)
    ext2_write_inode(ndir);
  return 0;
}

static struct inode *ext2_symlink(struct inode *dir, const char *name, size_t len, const char *target) {
  size_t tl = strlen(target);
  if (tl >= 4096)
    return ERR_PTR(-ENAMETOOLONG);
  struct inode *i = new_ext2_inode(dir, S_IFLNK | 0777);
  if (IS_ERR(i))
    return i;
  struct ext2_sb *e = E(dir->sb);
  if (tl < 60) {
    memcpy(EI(i)->block, target, tl);
  } else {
    int err;
    uint32_t b = bmap(i, 0, true, &err);
    if (!b) {
      i->nlink = 0;
      iput(i);
      return ERR_PTR(err ? err : -ENOSPC);
    }
    struct buf *bp = bread(e->b, b, e->bs);
    memset(bp->data, 0, e->bs);
    memcpy(bp->data, target, tl);
    bdirty(bp);
    brelse(bp);
  }
  i->size = tl;
  int r = dir_add(dir, name, len, (uint32_t)i->ino, i->mode);
  if (r) {
    i->nlink = 0;
    iput(i);
    return ERR_PTR(r);
  }
  ext2_write_inode(i);
  return i;
}

static ssize_t ext2_readlink(struct inode *i, char *buf, size_t n) {
  struct ext2_sb *e = E(i->sb);
  size_t len = MIN((size_t)i->size, n);
  if (fast_symlink(i)) {
    memcpy(buf, EI(i)->block, len);
    return (ssize_t)len;
  }
  int err;
  uint32_t b = bmap(i, 0, false, &err);
  if (!b)
    return -EIO;
  struct buf *bp = bread(e->b, b, e->bs);
  memcpy(buf, bp->data, MIN(len, (size_t)e->bs));
  brelse(bp);
  return (ssize_t)MIN(len, (size_t)e->bs);
}

/* last reference to an unlinked inode: free its blocks and number */
static void ext2_evict(struct inode *i) {
  struct ext2_sb *e = E(i->sb);
  if (i->sb->rdonly)
    return;
  bool dir = S_ISDIR(i->mode);
  if (!(S_ISLNK(i->mode) && fast_symlink(i)) && !S_ISCHR(i->mode) && !S_ISBLK(i->mode))
    free_blocks_from(i, 0);
  if (EI(i)->file_acl) {
    free_block(e, EI(i)->file_acl);
    EI(i)->file_acl = 0;
  }
  i->size = 0;
  i->blocks = 0;
  memset(EI(i)->block, 0, sizeof(EI(i)->block));
  ext2_write_inode(i);
  unsigned off;
  struct buf *bp = inode_buf(e, i->ino, &off);
  uint64_t now = realtime_ns() / 1000000000UL;
  wr32(bp->data + off + 20, (uint32_t)now); /* dtime */
  bdirty(bp);
  brelse(bp);
  free_inode_bit(e, (uint32_t)i->ino, dir);
}

static int ext2_statfs(struct super *sb, struct statfs64k *st) {
  struct ext2_sb *e = E(sb);
  st->bsize = st->frsize = e->bs;
  st->blocks = e->blocks - e->first_data;
  st->bfree = rd32(e->raw + 12);
  uint32_t res = rd32(e->raw + 8);
  st->bavail = st->bfree > res ? st->bfree - res : 0;
  st->files = e->inodes;
  st->ffree = rd32(e->raw + 16);
  st->namelen = 255;
  return 0;
}

static struct super *ext2_mount(struct blockdev *b, int flags, const char *data) {
  (void)data;
  uint8_t raw[1024];
  if (b->read(b, 2, 2, raw))
    return ERR_PTR(-EIO);
  if (rd16(raw + 56) != EXT2_MAGIC)
    return ERR_PTR(-EINVAL);
  struct ext2_sb *e = kzalloc(sizeof(*e));
  memcpy(e->raw, raw, 1024);
  e->b = b;
  e->bs = 1024U << rd32(raw + 24);
  if (rd32(raw + 24) > 2) { /* blocks larger than a page (the page cache assumes not) */
    printk("ext2: %u-byte blocks not supported\n", e->bs);
    kfree(e);
    return ERR_PTR(-EINVAL);
  }
  e->inodes = rd32(raw);
  e->blocks = rd32(raw + 4);
  e->first_data = rd32(raw + 20);
  e->bpg = rd32(raw + 32);
  e->ipg = rd32(raw + 40);
  uint32_t rev = rd32(raw + 76);
  e->isize = rev ? rd16(raw + 88) : 128;
  e->first_ino = rev ? rd32(raw + 84) : 11;
  e->incompat = rev ? rd32(raw + 96) : 0;
  e->rocompat = rev ? rd32(raw + 100) : 0;
  if (e->bs > 65536 || e->bpg == 0 || e->ipg == 0 || e->isize < 128) {
    kfree(e);
    return ERR_PTR(-EINVAL);
  }
  if (e->incompat & ~(uint32_t)INCOMPAT_FILETYPE) {
    printk("ext2: %s: unsupported features %#x\n", b->name, e->incompat & ~INCOMPAT_FILETYPE);
    kfree(e);
    return ERR_PTR(-EINVAL);
  }
  e->ngroups = (e->blocks - e->first_data + e->bpg - 1) / e->bpg;
  e->gd_blocks = (e->ngroups * 32 + e->bs - 1) / e->bs;
  e->gd = kmalloc(e->gd_blocks * e->bs);
  for (unsigned i = 0; i < e->gd_blocks; i++) {
    struct buf *bp = bread(b, e->first_data + 1 + i, e->bs);
    memcpy(e->gd + i * e->bs, bp->data, e->bs);
    brelse(bp);
  }
  struct super *sb = super_alloc(find_fs("ext2"), b);
  sb->priv = e;
  sb->ops = &ext2_sops;
  sb->blocksize = e->bs;
  sb->magic = EXT2_MAGIC;
  e->sb = sb;
  sb->rdonly = (flags & MS_RDONLY) || (e->rocompat & ~(uint32_t)(RO_SPARSE | RO_LARGE | RO_BTREE));
  if (sb->rdonly && !(flags & MS_RDONLY))
    printk("ext2: %s: read-only features %#x: mounted read-only\n", b->name, e->rocompat);
  struct inode *root = iget(sb, ROOT_INO);
  if (IS_ERR(root)) {
    kfree(e->gd);
    kfree(e);
    kfree(sb);
    return ERR_PTR(-EIO);
  }
  sb->root = root;
  if (!sb->rdonly) {
    /* the state: "valid" (clean) only when unmounted or shut down
       properly, so that e2fsck -p checks a disk the system did not stop
       cleanly; one that was not clean stays so until e2fsck has checked
       it (as Linux does) */
    uint16_t state = rd16(e->raw + 58);
    e->mount_state = state;
    if (!(state & 1) || (state & 2))
      printk("ext2: %s was not unmounted cleanly%s: check it with e2fsck\n", b->name,
             state & 2 ? " and has errors" : "");
    wr16(e->raw + 58, (uint16_t)(state & ~1));
    wr16(e->raw + 52, (uint16_t)(rd16(e->raw + 52) + 1)); /* mount count */
    wr32(e->raw + 44, (uint32_t)(realtime_ns() / 1000000000UL));
    e->dirty = true;
  }
  return sb;
}

static const struct inode_ops ext2_dir_iops = {
    .lookup = ext2_lookup,
    .create = ext2_create,
    .link = ext2_link,
    .unlink = ext2_unlink,
    .rmdir = ext2_rmdir,
    .rename = ext2_rename,
    .symlink = ext2_symlink,
};
static const struct inode_ops ext2_file_iops = {.truncate = ext2_truncate, .rawio = ext2_rawio};
static const struct inode_ops ext2_link_iops = {.readlink = ext2_readlink};
static const struct inode_ops ext2_special_iops = {.truncate = NULL};
static const struct file_ops ext2_dir_fops = {.readdir = ext2_readdir, .fsync = ext2_fsync, .seekable = true};
static const struct file_ops ext2_file_fops = {
    .read = ext2_read, .write = ext2_write, .fsync = ext2_fsync, .seekable = true};
static const struct super_ops ext2_sops = {
    .read_inode = ext2_read_inode,
    .write_inode = ext2_write_inode,
    .evict = ext2_evict,
    .put = ext2_put,
    .statfs = ext2_statfs,
    .sync = ext2_sync,
    .umount = ext2_umount,
    .shutdown = ext2_shutdown,
};

static struct fs_type ext2_type = {.name = "ext2", .needs_dev = true, .mount = ext2_mount};

void ext2_register(void) { register_fs(&ext2_type); }
