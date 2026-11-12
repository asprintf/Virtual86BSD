# makefs — make an FFS2 image from a directory tree (V86BSD)

    makefs [-Uv] [-b free-blocks] [-f free-files] [-M minimum-size]
           [-m maximum-size] [-o fs-option[,fs-option ...]] [-s image-size]
           [-T time] image-file directory

`makefs` writes the files under *directory* into a new FFS2 file system in
*image-file*, without root and without mounting anything: the way to
make the disk image of a root file system on the machine that builds the
system. The result is what `newfs` makes plus the files, so `fsck_ffs`
calls it clean and the kernel mounts it.

* `-s size` makes the image exactly that big. Without it the size is worked
  out from the tree (and `-b`, `-f`, `-M`, `-m`, `minfree`); if that is not
  enough room, `makefs` says so: give a bigger `-s` or `-b`.
* `-b n` and `-f n` leave n bytes and n inodes free besides what the tree
  needs; with a `%` (`-b 10%`) that many percent more. Sizes take the
  suffixes `k`, `m`, `g`, `t` (and `b` for 512, `w` for 4, `2x3` products).
* `-M`, `-m` are the least and the most the size may be.
* `-U` makes every file belong to root (uid 0, gid 0), whoever owns it in
  the tree: the tree of a build belongs to the user who built it.
* `-T seconds` stamps every file and the file system with that time (since
  the epoch), so that two runs give the same image.
* `-o` options of the file system: `bsize`, `fsize` (16384 and 2048 as
  `newfs`), `density` (bytes per inode), `minfree` (5), `maxbpg`,
  `avgfilesize`, `avgfpdir`, `extent`, `maxbpcg`, `optimization=time|space`
  and `label`.
* Only FFS2 is made, and with the plain magic number (not NetBSD's
  extended attribute variant), no cluster maps (the driver keeps none), the
  byte order of the host. Hard links, symbolic links (short ones in the
  inode), directories of any size, sparse files (written out as zeros),
  device nodes, FIFOs and sockets are kept; the tree is read once, in name
  order, so the numbering of the inodes does not depend on the host's
  directory order.

## Where it comes from

NetBSD's `usr.sbin/makefs` writes an FFS image with the allocator of
NetBSD's kernel (`ffs_balloc`, `ffs_alloc`), run on buffers that are blocks
of the image file. Those files, the file system headers and `mkfs.c` are
NetBSD's, BSD-licensed (three clauses), kept with their notices and the
"Adapted for V86BSD" lines (the table of adapted files is
`share/doc` once it exists):

| here | NetBSD |
|---|---|
| `ffs/ffs_alloc.c`, `ffs/ffs_balloc.c`, `ffs/ufs_bmap.c`, `ffs/ffs_extern.h`, `ffs/newfs_extern.h`, `ffs/ufs_inode.h` | `usr.sbin/makefs/ffs/` |
| `ffs/mkfs.c` | `usr.sbin/makefs/ffs/mkfs.c` (FFS2 super block at 64 KiB, the old geometry fields `fsck_ffs` checks) |
| `ufs/ufs/{dinode,dir,quota,ufs_bswap}.h`, `ufs/ffs/{fs.h,ffs_extern.h,ffs_subr.c,ffs_tables.c,ffs_bswap.c}` | `sys/ufs/ufs/`, `sys/ufs/ffs/` |

These are NetBSD's own headers, kept apart (`-I.` puts them before the
system's) because they differ from the OpenBSD ones of `sys/ufs`.

The rest is written for V86BSD, and not from NetBSD's makefs: the front end
(`makefs.c`), the tree reader (`walk.c`), the image builder (`image.c`: the
size, the directories, the inodes) and the buffers (`ffs/buf.c`), because
those files of NetBSD's makefs carry the four-clause Wasabi Systems licence,
which this system does not take. `misc.c` and `nbtool_config.h` give the
NetBSD library functions the allocator calls (`emalloc` and its relatives,
`strsuftoll`, `strsep`).
