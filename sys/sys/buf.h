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
int bwritenow(struct buf *bp);          /* written to the device now */
void brelse(struct buf *bp);
void bsync(struct blockdev *b);        /* may let others run meanwhile */
void bsync_atomic(struct blockdev *b); /* does not */
void bforget(struct blockdev *b, uint64_t block, unsigned size);
void bcache_init(void);
uint64_t bcache_reclaim(uint64_t npages); /* pages freed */
uint64_t bcache_kb(void);

#endif
