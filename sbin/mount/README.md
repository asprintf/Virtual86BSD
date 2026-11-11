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

* No arguments: list the mounts (`/dev/disk0s1 on / type ext2 (local)`, from `/proc/self/mounts`, the table `df`
  reads too: `lib/libxcu/mnttab.c`).
* `-a` mounts what `/etc/fstab` lists, skipping `noauto` entries, `swap`/`sw`/`xx` and what is mounted already
  (with `-u` it updates them). `mount node` or `mount special` takes the rest from the fstab.
* `-u` changes a mount that is there (a remount): `mount -uw /` makes `/` writable, `-ur` read-only.
* Options: `ro`, `rw`, `noauto`, `update`, `defaults`. The kernel has no others (no `nosuid`, `noexec`), so any
  other word is an error rather than silently ignored.
* With no `-t`, a `/dev/` special is `ext2` and anything else `tmpfs`.

## Testing

`test_mount.sh` (root, on V86BSD only) mounts tmpfs on directories of its own: by hand, read-only and back
with `-u`, with an fstab (it saves and restores `/etc/fstab`), the list and `-t`, and every error.
