/* pcache.c - the page cache of files on block file systems (ext2).

   Shared mappings are what make it: the pages a MAP_SHARED mapping of a
   file maps are the file's cached pages, so every process that maps the
   file sees the same memory.  read() and write() use a cached page where
   there is one, so they agree with the mappings (POSIX: a change made
   through a shared mapping is visible to read(), and a write() to the
   mapping).  Pages wholly beyond the end of the file are not in it:
   touching them is SIGBUS.

   A page a shared mapping has mapped is a candidate for write-back
   (SHM_CAND): pcache_sync() compares it with the file and writes it where
   it differs, setting the file's times.  That happens on msync, fsync,
   sync, the periodic flush, the last unmapping, and when the inode goes
   away.  Comparing instead of tracking dirty bits needs no reverse
   mappings, and costs a memcmp per mapped page.

   The cache lives while something maps the file: the last unmapping
   writes it back and frees it.  The file system provides iops->rawio. */
#include "kernel.h"
#include "mm.h"
#include "fs.h"
#include "proc.h"

struct shmobj *pcache_get(struct inode *i) {
  if (!i->iops || !i->iops->rawio)
    return NULL;
  if (!i->pcache) {
    struct shmobj *o = shmobj_new(PAGE_UP(i->size) >> 12);
    if (!o)
      return NULL;
    o->owner = i; /* the inode's reference */
    i->pcache = o;
  }
  shmobj_get(i->pcache);
  return i->pcache;
}

/* page idx, read from the file (shmobj_page for a page cache) */
uint64_t pcache_fill(struct shmobj *o, size_t idx) {
  struct inode *i = o->owner;
  uint64_t pos = (uint64_t)idx << 12;
  size_t n = i->size > pos ? (size_t)MIN(PAGE_SIZE, i->size - pos) : 0;
  uint64_t phys = alloc_page();
  if (!phys)
    return 0;
  memset((char *)P2V(phys) + n, 0, PAGE_SIZE - n);
  if (n && i->iops->rawio(i, P2V(phys), n, pos, false)) {
    page_put(phys);
    return 0;
  }
  /* reading may have slept: the file may have shrunk, or the page come */
  if (idx >= o->npages || o->page[idx]) {
    page_put(phys);
    return idx < o->npages ? o->page[idx] & SHM_ADDR : 0;
  }
  o->page[idx] = phys;
  return phys;
}

void *pcache_lookup(struct inode *i, uint64_t pos) {
  struct shmobj *o = i->pcache;
  size_t idx = (size_t)(pos >> 12);
  if (!o || idx >= o->npages || !o->page[idx])
    return NULL;
  return P2V(o->page[idx] & SHM_ADDR);
}

/* the size changed (write, truncate): pages beyond the end leave the cache
   and every mapping (touching them is then SIGBUS), the bytes beyond the
   end in the last page are zero */
void pcache_resize(struct inode *i) {
  struct shmobj *o = i->pcache;
  if (!o)
    return;
  size_t np = (size_t)(PAGE_UP(i->size) >> 12);
  if (np < o->npages)
    shmobj_unmap_from(o, np);
  shmobj_resize(o, np);
  if ((i->size & (PAGE_SIZE - 1)) && np && o->page[np - 1]) {
    size_t keep = (size_t)(i->size & (PAGE_SIZE - 1));
    memset((char *)P2V(o->page[np - 1] & SHM_ADDR) + keep, 0, PAGE_SIZE - keep);
  }
}

int pcache_sync(struct inode *i) {
  struct shmobj *o = i->pcache;
  if (!o || i->sb->rdonly)
    return 0;
  shmobj_get(o); /* rawio may sleep */
  uint64_t tmp = 0;
  int err = 0;
  bool changed = false;
  for (size_t idx = 0; idx < o->npages; idx++) {
    uint64_t ent = o->page[idx];
    uint64_t pos = (uint64_t)idx << 12;
    if (!(ent & SHM_CAND))
      continue;
    if (pos >= i->size)
      break;
    size_t n = (size_t)MIN(PAGE_SIZE, i->size - pos);
    if (!tmp && !(tmp = alloc_page())) {
      err = -ENOMEM;
      break;
    }
    int r = i->iops->rawio(i, P2V(tmp), n, pos, false);
    if (r == 0 && idx < o->npages && o->page[idx] == ent &&
        memcmp(P2V(tmp), P2V(ent & SHM_ADDR), n) != 0) {
      r = i->iops->rawio(i, P2V(ent & SHM_ADDR), n, pos, true);
      if (r == 0)
        changed = true;
    }
    if (r && !err)
      err = r;
  }
  if (tmp)
    page_put(tmp);
  if (changed)
    inode_set_times(i, false, true, true);
  shmobj_put(o); /* the inode still holds it */
  return err;
}

static void drop(struct inode *i) {
  struct shmobj *o = i->pcache;
  i->pcache = NULL;
  o->owner = NULL;
  shmobj_put(o);
}

void pcache_unmapped(struct shmobj *o) {
  struct inode *i = o->owner;
  if (!i || o->ref != 1)
    return;
  pcache_sync(i);
  if (i->pcache == o && o->ref == 1)
    drop(i);
}

void pcache_release(struct inode *i) {
  if (!i->pcache)
    return;
  if (i->nlink) /* an unlinked file going away needs no write-back (an
                   unlinked one still open or mapped does: pcache_sync) */
    pcache_sync(i);
  if (i->pcache)
    drop(i);
}
