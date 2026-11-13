# Credits: code adapted from the BSDs

V86BSD is a fork of Shall; part of its BSD side is code that was *adapted*
from OpenBSD and NetBSD, not written here. Each such file keeps the
original copyright and licence text word for word and has, after it, a
note `Adapted for V86BSD from OpenBSD|NetBSD <path> (revision N). Changes:
...` that names the source file, its revision and what was changed. This
table is made from those notes (`grep -rl "Adapted for V86BSD from"`);
the note in the file is the authority.

Sources were chosen by their licences: every file below carries the
BSD licence of the Regents of the University of California or a
similar one (three or two clauses) or, for one file, the ISC licence;
**none has an advertising clause**, and none is under the GPL.
Checked on 2026-11-13 by reading the licence text of each file. What was
left out for its licence: NetBSD's `makefs` front end (`makefs.c`, `walk.c`,
`ffs.c` and their headers: four clauses, Wasabi Systems), OpenBSD's
`ffs_softdep.c` (a special licence), and Linux's `ufs` and e2fsprogs
(GPL). `makefs`'s front end, tree reader, image builder and buffers were
written for V86BSD instead.

Not in the table: code written here that only follows a BSD's interface
or behaviour (`err(3)`, `getprogname(3)`, `arc4random(3)`, `syslogd`,
`ifconfig`, `route`, ...: compared with the BSDs' manual pages, not copied)
and the Shall code the fork started from (see `LICENSE`).

| here | from | revision | licence |
|---|---|---|---|
| `include/fstab.h` | OpenBSD `include/fstab.h` | 1.5 | BSD 3-clause |
| `lib/libc/misc/fstab.c` | OpenBSD `lib/libc/gen/fstab.c` | 1.22 | BSD 3-clause |
| `lib/libc/stdlib/strtonum.c` | OpenBSD `lib/libc/stdlib/strtonum.c` | 1.8 | ISC |
| `sbin/dumpfs/dumpfs.c` | OpenBSD `sbin/dumpfs/dumpfs.c` | 1.39 | BSD 3-clause |
| `sbin/fsck/fsck.c` | OpenBSD `sbin/fsck/fsck.c` | 1.41 | BSD 3-clause |
| `sbin/fsck/fsutil.c` | OpenBSD `sbin/fsck/fsutil.c` | 1.25 | BSD 3-clause |
| `sbin/fsck/fsutil.h` | OpenBSD `sbin/fsck/fsutil.h` | 1.9 | BSD 2-clause |
| `sbin/fsck/opendev.c` | OpenBSD `lib/libutil/opendev.c` | 1.18 | BSD 2-clause |
| `sbin/fsck/pathnames.h` | OpenBSD `sbin/fsck/pathnames.h` | 1.2 | BSD 2-clause |
| `sbin/fsck/preen.c` | OpenBSD `sbin/fsck/preen.c` | 1.20 | BSD 3-clause |
| `sbin/fsck_ffs/dir.c` | OpenBSD `sbin/fsck_ffs/dir.c` | 1.36 | BSD 3-clause |
| `sbin/fsck_ffs/extern.h` | OpenBSD `sbin/fsck_ffs/extern.h` | 1.15 | BSD 2-clause |
| `sbin/fsck_ffs/fsck.h` | OpenBSD `sbin/fsck_ffs/fsck.h` | 1.36 | BSD 3-clause |
| `sbin/fsck_ffs/inode.c` | OpenBSD `sbin/fsck_ffs/inode.c` | 1.52 | BSD 3-clause |
| `sbin/fsck_ffs/main.c` | OpenBSD `sbin/fsck_ffs/main.c` | 1.55 | BSD 3-clause |
| `sbin/fsck_ffs/pass1.c` | OpenBSD `sbin/fsck_ffs/pass1.c` | 1.50 | BSD 3-clause |
| `sbin/fsck_ffs/pass1b.c` | OpenBSD `sbin/fsck_ffs/pass1b.c` | 1.22 | BSD 3-clause |
| `sbin/fsck_ffs/pass2.c` | OpenBSD `sbin/fsck_ffs/pass2.c` | 1.39 | BSD 3-clause |
| `sbin/fsck_ffs/pass3.c` | OpenBSD `sbin/fsck_ffs/pass3.c` | 1.18 | BSD 3-clause |
| `sbin/fsck_ffs/pass4.c` | OpenBSD `sbin/fsck_ffs/pass4.c` | 1.26 | BSD 3-clause |
| `sbin/fsck_ffs/pass5.c` | OpenBSD `sbin/fsck_ffs/pass5.c` | 1.52 | BSD 3-clause |
| `sbin/fsck_ffs/setup.c` | OpenBSD `sbin/fsck_ffs/setup.c` | 1.72 | BSD 3-clause |
| `sbin/fsck_ffs/utilities.c` | OpenBSD `sbin/fsck_ffs/utilities.c` | 1.55 | BSD 3-clause |
| `sbin/newfs/mkfs.c` | OpenBSD `sbin/newfs/mkfs.c` | 1.103 | BSD 3-clause |
| `sbin/newfs/newfs.c` | OpenBSD `sbin/newfs/newfs.c` | 1.120 | BSD 3-clause |
| `sbin/newfs/scan_scaled.c` | OpenBSD `lib/libutil/fmt_scaled.c` | 1.26 | BSD 3-clause |
| `sbin/tunefs/tunefs.c` | OpenBSD `sbin/tunefs/tunefs.c` | 1.41 | BSD 3-clause |
| `sys/sys/queue.h` | OpenBSD `sys/sys/queue.h` | 1.47 | BSD 3-clause |
| `sys/ufs/ffs/ffs_alloc.c` | OpenBSD `sys/ufs/ffs/ffs_alloc.c` | 1.115 | BSD 3-clause |
| `sys/ufs/ffs/ffs_balloc.c` | OpenBSD `sys/ufs/ffs/ffs_balloc.c` | 1.47 | BSD 3-clause |
| `sys/ufs/ffs/ffs_subr.c` | OpenBSD `sys/ufs/ffs/ffs_subr.c` | 1.37 | BSD 3-clause |
| `sys/ufs/ffs/ffs_tables.c` | OpenBSD `sys/ufs/ffs/ffs_tables.c` | 1.6 | BSD 3-clause |
| `sys/ufs/ffs/ffs_vfsops.c` | OpenBSD `sys/ufs/ffs/ffs_vfsops.c` | 1.201 | BSD 3-clause |
| `sys/ufs/ffs/ffs_vnops.c` | OpenBSD `sys/ufs/ufs/ufs_lookup.c` | 1.61 | BSD 3-clause |
| `sys/ufs/ffs/fs.h` | OpenBSD `sys/ufs/ffs/fs.h` | 1.46 | BSD 3-clause |
| `sys/ufs/ufs/dinode.h` | OpenBSD `sys/ufs/ufs/dinode.h` | 1.19 | BSD 3-clause |
| `sys/ufs/ufs/dir.h` | OpenBSD `sys/ufs/ufs/dir.h` | 1.13 | BSD 3-clause |
| `usr.sbin/makefs/ffs/ffs_alloc.c` | NetBSD `usr.sbin/makefs/ffs/ffs_alloc.c` | 1.34 | BSD 3-clause |
| `usr.sbin/makefs/ffs/ffs_balloc.c` | NetBSD `usr.sbin/makefs/ffs/ffs_balloc.c` | 1.23 | BSD 3-clause |
| `usr.sbin/makefs/ffs/ffs_extern.h` | NetBSD `usr.sbin/makefs/ffs/ffs_extern.h` | 1.9 | BSD 3-clause |
| `usr.sbin/makefs/ffs/mkfs.c` | NetBSD `usr.sbin/makefs/ffs/mkfs.c` | 1.43 | BSD 3-clause |
| `usr.sbin/makefs/ffs/newfs_extern.h` | NetBSD `usr.sbin/makefs/ffs/newfs_extern.h` | 1.5 | BSD 2-clause |
| `usr.sbin/makefs/ffs/ufs_bmap.c` | NetBSD `usr.sbin/makefs/ffs/ufs_bmap.c` | 1.19 | BSD 3-clause |
| `usr.sbin/makefs/ffs/ufs_inode.h` | NetBSD `usr.sbin/makefs/ffs/ufs_inode.h` | 1.5 | BSD 3-clause |
| `usr.sbin/makefs/ufs/ffs/ffs_bswap.c` | NetBSD `sys/ufs/ffs/ffs_bswap.c` | 1.40 | BSD 2-clause |
| `usr.sbin/makefs/ufs/ffs/ffs_extern.h` | NetBSD `sys/ufs/ffs/ffs_extern.h` | 1.88 | BSD 3-clause |
| `usr.sbin/makefs/ufs/ffs/ffs_subr.c` | NetBSD `sys/ufs/ffs/ffs_subr.c` | 1.54 | BSD 3-clause |
| `usr.sbin/makefs/ufs/ffs/ffs_tables.c` | NetBSD `sys/ufs/ffs/ffs_tables.c` | 1.9 | BSD 3-clause |
| `usr.sbin/makefs/ufs/ffs/fs.h` | NetBSD `sys/ufs/ffs/fs.h` | 1.73 | BSD 3-clause |
| `usr.sbin/makefs/ufs/ufs/dinode.h` | NetBSD `sys/ufs/ufs/dinode.h` | 1.25 | BSD 3-clause |
| `usr.sbin/makefs/ufs/ufs/dir.h` | NetBSD `sys/ufs/ufs/dir.h` | 1.27 | BSD 3-clause |
| `usr.sbin/makefs/ufs/ufs/quota.h` | NetBSD `sys/ufs/ufs/quota.h` | 1.30 | BSD 3-clause |
| `usr.sbin/makefs/ufs/ufs/ufs_bswap.h` | NetBSD `sys/ufs/ufs/ufs_bswap.h` | 1.23 | BSD 2-clause |

## Test suites in the tree

* `regress/os-test`: Sortix's os-test (https://gitlab.com/sortix/os-test),
  git 8e790a4 (19 October 2026), ISC licence (`regress/os-test/LICENSE`),
  copyright Jonas 'Sortie' Termansen and contributors. Imported unchanged;
  what V86BSD added there (its own suites, a line in `misc/suites.list`) is
  listed in `regress/os-test/README.V86BSD`.

## Licences in short

* **BSD 3-clause / 2-clause**: redistribution with the notice kept in source
  and in the documentation of a binary distribution; the name of the
  authors may not be used to promote the product (three clauses).
* **ISC** (`lib/libc/stdlib/strtonum.c`, `regress/os-test`): permission to use, copy, modify
  and distribute with the notice kept.

The notices themselves are in the files, and a binary distribution of this
system (an image from `make release`) carries them in `/usr/src`.
