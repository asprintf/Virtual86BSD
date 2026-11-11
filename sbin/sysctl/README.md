# sysctl — get and set kernel state (V86BSD)

`sysctl` of OpenBSD, for the nodes the kernel answers (`sys/kern/kern_sysctl.c`).

    sysctl [-Aanq] [-f file] [name[=value] ...]

## Build and test

    make           # builds ./sysctl
    make test      # the tests (sh test_sysctl.sh)
    make install   # into $(BINDIR); DESTDIR= for staging
    make clean

## What is implemented

* `name` prints `name=value` (`-n`: the value alone); `name=value` sets it (root) and prints
  `name: old -> new` (`-q`: nothing). A name that is a node (`kern`, `hw`) prints everything below it; no
  name, `-a` and `-A` print every node the kernel answers. `-f file` reads `name=value` lines (`#` starts
  a comment).
* The names come from `<sys/sysctl.h>`: all of OpenBSD's are known, the program lists and prints the ones
  the kernel answers: `kern` (ostype, osrelease, osversion, version, hostname, domainname, boottime, clockrate,
  maxproc, maxfiles, argmax, posix1version, ngroups, nprocs, nthreads ...), `hw` (machine, model, ncpu,
  byteorder, pagesize, physmem, disknames, diskcount, cpuspeed ...) and `vm.loadavg`. `net.*` is reserved.
* Writable: `kern.hostname` and `kern.domainname` (at most 64 bytes).
* Native only: on the Linux ABI `sysctl(2)` is ENOSYS.
* Differences from OpenBSD: the exit status is 1 if a name could not be read or set (OpenBSD's is always 0);
  there are no sub-trees (`kern.proc`, `kern.malloc`, `hw.sensors` ...).

## Testing

`test_sysctl.sh` reads every kind of value (string, int, quad, the three structures), lists
nodes and `-a`, sets the host and domain name (as root, and puts them back), reads a file with comments,
and checks every error message. It is skipped where there is no `sysctl(2)`. The kernel side is
`regress/sys/kern/k_sysctl.c`.
