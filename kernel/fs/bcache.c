/* bcache.c - block devices and the buffer cache (write-back, LRU).  Dirty
   buffers are written when evicted, by sync, and every few seconds by the
   flush thread.

   The cache grows with use up to half of the memory, as long as a reserve
   (a sixteenth of the memory) stays free for everything else; beyond that
   its least recently used buffers are reused.  When the page allocator runs
   out, it takes memory back from the cache (bcache_reclaim). */
#include "kernel.h"
#include "arch.h"
#include "mm.h"
#include "proc.h"
#include "dev.h"

#define NBLOCKDEV 16
#define HASH 16384
#define MIN_BUFS 8192

static struct blockdev *bdevs[NBLOCKDEV];
static struct list lru = LIST_INIT(lru); /* most recent first */
static struct list dirtyq = LIST_INIT(dirtyq); /* the dirty buffers, oldest first */
static struct list hash[HASH];
static int nbufs;
static uint64_t max_bufs = MIN_BUFS, reserve_pages = 1024;
static uint64_t cache_bytes;  /* the data of the buffers */
static bool reclaiming;
static int bwrite(struct buf *bp);

uint64_t bcache_kb(void) { return cache_bytes / 1024; }

void blockdev_register(struct blockdev *b) {
  for (int i = 0; i < NBLOCKDEV; i++)
    if (!bdevs[i]) {
      bdevs[i] = b;
      printk("%s: %lu sectors (%lu MiB)\n", b->name, b->sectors, b->sectors >> 11);
      return;
    }
}

struct blockdev *blockdev_get(dev_t dev) {
  for (int i = 0; i < NBLOCKDEV; i++)
    if (bdevs[i] && bdevs[i]->dev == dev)
      return bdevs[i];
  return NULL;
}

struct blockdev *blockdev_by_name(const char *name) {
  for (int i = 0; i < NBLOCKDEV; i++)
    if (bdevs[i] && !strcmp(bdevs[i]->name, name))
      return bdevs[i];
  return NULL;
}

struct blockdev *blockdev_nth(int i) { return i < NBLOCKDEV ? bdevs[i] : NULL; }

void bcache_init(void) {
  for (int i = 0; i < HASH; i++)
    list_init(&hash[i]);
  if (total_pages / 2 > max_bufs)
    max_bufs = total_pages / 2;
  if (total_pages / 16 > reserve_pages)
    reserve_pages = total_pages / 16;
}

/* Frees unused buffers, least recently used first (a dirty one is written
   first), until npages pages are free or none is left; for the page
   allocator.  Returns the number of pages freed. */
uint64_t bcache_reclaim(uint64_t npages) {
  if (reclaiming)
    return 0;
  reclaiming = true;
  uint64_t freed = 0;
  /* clean buffers first; dirty ones (written first) only if that is not
     enough */
  for (int pass = 0; pass < 2; pass++)
  for (struct list *p = lru.prev; p != &lru && freed < npages;) {
    struct buf *o = list_entry(p, struct buf, lru);
    p = p->prev;
    if (o->ref || (o->dirty && pass == 0))
      continue;
    if (o->dirty && bwrite(o))
      continue;
    list_del(&o->hash);
    list_del(&o->lru);
    cache_bytes -= o->size;
    freed += (o->size + PAGE_SIZE - 1) / PAGE_SIZE;
    kfree(o->data);
    kfree(o);
    nbufs--;
  }
  reclaiming = false;
  return freed;
}

static unsigned hkey(struct blockdev *b, uint64_t block) {
  return (unsigned)((block * 2654435761U) ^ (uint64_t)(uintptr_t)b) % HASH;
}

static int bwrite(struct buf *bp) {
  unsigned spb = bp->size / 512;
  int e = bp->bdev->write(bp->bdev, bp->block * spb, spb, bp->data);
  bp->bdev->unflushed = true;
  if (e == 0) {
    bp->dirty = false;
    list_del(&bp->dirty_link);
  } else
    printk("%s: write error at block %lu\n", bp->bdev->name, bp->block);
  return e;
}

static struct buf *find(struct blockdev *b, uint64_t block, unsigned size) {
  list_for_each(p, &hash[hkey(b, block)]) {
    struct buf *bp = list_entry(p, struct buf, hash);
    if (bp->bdev == b && bp->block == block && bp->size == size)
      return bp;
  }
  return NULL;
}

struct buf *bget(struct blockdev *b, uint64_t block, unsigned size) {
  struct buf *bp = find(b, block, size);
  if (bp) {
    bp->ref++;
    list_del(&bp->lru);
    list_add(&lru, &bp->lru);
    return bp;
  }
  if ((uint64_t)nbufs >= max_bufs || free_pages_count < reserve_pages) {
    /* reuse the least recently used free buffer of the same size, a
       clean one if there is one (a dirty one must be written first) */
    for (int pass = 0; pass < 2 && !bp; pass++) {
      int seen = 0;
      for (struct list *p = lru.prev; p != &lru; p = p->prev) {
        struct buf *o = list_entry(p, struct buf, lru);
        if (pass == 0 && ++seen > 256)
          break; /* (only near the end of the list: then any) */
        if (o->ref || o->size != size || (o->dirty && pass == 0))
          continue;
        if (o->dirty && bwrite(o))
          list_del(&o->dirty_link); /* (lost: reported) */
        list_del(&o->hash);
        list_del(&o->lru);
        bp = o;
        break;
      }
    }
  }
  if (!bp) {
    bp = kzalloc(sizeof(*bp));
    if (!bp)
      panic("buffer cache: out of memory");
    bp->data = kmalloc(size);
    if (!bp->data)
      panic("buffer cache: out of memory");
    nbufs++;
    cache_bytes += size;
  }
  bp->bdev = b;
  bp->block = block;
  bp->size = size;
  bp->ref = 1;
  bp->dirty = false;
  bp->valid = false;
  list_add(&hash[hkey(b, block)], &bp->hash);
  list_add(&lru, &bp->lru);
  return bp;
}

struct buf *bread(struct blockdev *b, uint64_t block, unsigned size) {
  struct buf *bp = bget(b, block, size);
  if (!bp->valid) {
    unsigned spb = size / 512;
    if (b->read(b, block * spb, spb, bp->data)) {
      printk("%s: read error at block %lu\n", b->name, block);
      memset(bp->data, 0, size);
    }
    bp->valid = true;
  }
  return bp;
}

void bdirty(struct buf *bp) {
  if (!bp->dirty)
    list_add_tail(&dirtyq, &bp->dirty_link);
  bp->dirty = true;
  bp->valid = true;
}

void brelse(struct buf *bp) {
  assert(bp->ref > 0);
  bp->ref--;
}

/* the write caches of the devices written to (all, or b) */
static void flush_devices(struct blockdev *b) {
  for (int i = 0; i < NBLOCKDEV; i++) {
    struct blockdev *d = bdevs[i];
    if (d && (!b || d == b) && d->unflushed && d->flush) {
      d->unflushed = false;
      if (d->flush(d))
        printk("%s: cache flush failed\n", d->name);
    }
  }
}

/* Writes the buffers dirty now (of b, or all), oldest first.  Writing
   many takes long, and the kernel is not preemptive: unless the caller is
   in the middle of something others must not see (may_yield false), it
   lets them run between buffers when their time has come.  It takes the
   first buffer of the list each time, so what others do meanwhile cannot
   upset it; buffers that become dirty meanwhile may wait for the next
   time. */
static void do_bsync(struct blockdev *b, bool may_yield) {
  size_t n = 0;
  list_for_each(p, &dirtyq) n++;
  while (n-- > 0 && !list_empty(&dirtyq)) {
    struct buf *bp = list_entry(dirtyq.next, struct buf, dirty_link);
    if ((b && bp->bdev != b) || bwrite(bp)) {
      list_del(&bp->dirty_link); /* another device's, or a write error */
      list_add_tail(&dirtyq, &bp->dirty_link);
    }
    if (may_yield && need_resched)
      yield();
  }
  flush_devices(b);
}

void bsync(struct blockdev *b) { do_bsync(b, true); }
void bsync_atomic(struct blockdev *b) { do_bsync(b, false); }

/* block of b is free now (a file shrank or went away): what the cache
   holds for it need not reach the disk (the block's next owner sets its
   contents first: ext2's alloc_block zeroes it) */
void bforget(struct blockdev *b, uint64_t block, unsigned size) {
  struct buf *bp = find(b, block, size);
  if (bp && bp->dirty) {
    bp->dirty = false;
    list_del(&bp->dirty_link);
  }
}

/* the flush thread: inodes and buffers every 5 seconds */
long flush_thread(void *arg) {
  (void)arg;
  for (;;) {
    sleep_ns(5000000000UL);
    extern void sync_all(void);
    sync_all(); /* (inodes, then bsync(NULL)) */
  }
  return 0;
}
