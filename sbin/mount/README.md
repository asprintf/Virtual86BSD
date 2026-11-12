# mount — mount file systems (V86BSD)

The `mount` of the BSDs, for what this kernel can do.

    mount [-t type[,type...]]
    mount -a [-t type[,type...]]
    mount [-ruvw] [-t type] [-o options] special node
    mount [-ruvw] [-o options] special | node

## Build and test

    make           # builds ./mount
    make test      # the tests (sh test_mount.sh)
    make install   # into $(BINDIR); DESTDIR= for staging
    make clean

## What is implemented

* No arguments: list the mounts (`/dev/disk0s1 on / type ffs (local)`, from `/proc/self/mounts`, the table `df`
  reads too: `lib/libxcu/mnttab.c`).
* `-a` mounts what `/etc/fstab` lists, skipping `noauto` entries, `swap`/`sw`/`xx` and what is mounted already
  (with `-u` it updates them). `mount node` or `mount special` takes the rest from the fstab.
* `-u` changes a mount that is there (a remount): `mount -uw /` makes `/` writable, `-ur` read-only.
* `-ur -o reload /` makes the kernel read a read-only file system again (the superblock, the summary and the
  inodes in use) after `fsck_ffs` changed its disk: what `fsck_ffs` does itself for the root. Only a read-only mount
  can be reloaded.
* Options: `ro`, `rw`, `noauto`, `update`, `reload` (with `-u`), `defaults`. The kernel has no others (no `nosuid`, `noexec`), so any
  other word is an error rather than silently ignored.
* With no `-t`, a `/dev/` special is `ffs` and anything else `tmpfs`.

## Testing

`test_mount.sh` (root, on V86BSD only) mounts tmpfs on directories of its own: by hand, read-only and back
with `-u`, with an fstab (it saves and restores `/etc/fstab`), the list and `-t`, and every error.
