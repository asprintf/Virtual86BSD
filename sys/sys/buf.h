/* buf.h - the buffer cache (kern/vfs_bio.c). */
#ifndef _SYS_BUF_H_
#define _SYS_BUF_H_

#include <sys/systm.h>
#include <sys/conf.h>

/* buffer cache (kern/vfs_bio.c) */
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

#endif
