# umount — unmount file systems (V86BSD)

The `umount` of the BSDs, for what this kernel can do.

    umount [-v] special | node ...
    umount -a [-v] [-t type[,type...]]

## Build and test

    make           # builds ./umount
    make test      # the tests (sh test_umount.sh)
    make install   # into $(BINDIR); DESTDIR= for staging
    make clean

## What is implemented

* A name is a directory or a special file; a path with `..` or a symbolic link is made absolute first.
* `-a` unmounts every mount, the last first, except `/`, `/dev` and `/proc`; `-t` keeps to the types listed.
* A mount that is busy (a mount below it, an open file or working directory) fails with its reason and the rest go on.
* No `-f`: the kernel has no forced unmount.

## Testing

`test_umount.sh` (root, on V86BSD only): by directory, by path with `..`, several at once, `-v`, `-a -t`,
and the busy cases.
