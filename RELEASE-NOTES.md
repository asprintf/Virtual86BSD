# Virtual/86 BSD 1.0 — release notes

Virtual/86 BSD (V86BSD) 1.0, November 2026.  It is a fork of Shall 0.9.1
(31 October – 6 November 2026), shaped after OpenBSD and made for virtual
machines: QEMU with KVM is the platform it is built and tested on.  This page
says what changed since Shall 0.9.1 and what is still missing.  The commit
log has the detail; the manuals are `README.md`, `sys/README.md` and
`share/doc/`.

## What it is

The POSIX.1-2024 utilities (all 155, with the XSI option), a C17 compiler,
a C library for x86-64 and a kernel, built by themselves.  The system is
built from a Linux host with `make release`, which makes a bootable disk
image (`obj/v86bsd.img`); `sh boot.sh` starts it in QEMU.  Root's password is
`root` (`ROOTPW=` sets another one when the image is made).

## The system, from 0.9.1 to 1.0

**A BSD-style system around the kernel**
* The system is now named and laid out like a BSD: `/bsd` is the kernel, the
  sources are in `/usr/src`, the utilities are in `bin`, `sbin`, `usr.bin`
  and `usr.sbin`, and `make buildworld`, `buildkernel`, `release` build it.
* The kernel is configured and attached the OpenBSD way: configuration
  files, autoconfiguration (`cpu0 at mainbus0`, `pci0`, `vioblk0`, `em0` …) and
  the boot messages `dmesg` shows.  The devices are the ones QEMU emulates:
  virtio disk (`vioblk`), virtio network (`vio0`, new), Intel e1000 (`em0`),
  IDE (`pciide`), 16550 serial console, PS/2 keyboard, VGA.
* The kernel still runs Linux programs through Linux's system call
  interface.

**Booting and running a multi-user system**
* `init`, `getty` (`/etc/gettytab`), `login`, `passwd`
  (`/etc/master.passwd`), `shutdown`, `halt`, `reboot`.
* `/etc/rc` and the daemons of `/etc/rc.d`, started and stopped with `rcctl`;
  `mount`/`umount`, `sysctl(8)`, `hostname`, `sync`, `dmesg`.
* `syslogd`, `/dev/klog`, `kern.msgbuf`; `login`, `init` and `shutdown` log to
  it.

**The FFS2 file system is the root**
* A new FFS2 driver, `newfs`, `fsck` (the front end) and `fsck_ffs`, `dumpfs`,
  `tunefs`, `makefs`; the code is adapted from OpenBSD and NetBSD and
  credited in `share/doc/`.  The disk image is made with `makefs`.
* The root is mounted read-only, `/etc/rc` runs `fsck -p`, and `mount -uw /`
  follows; if `fsck` changed the root, it is reloaded (`mount -o reload`)
  instead of asking for a reboot.  A damaged FFS stays read-only.
* Metadata that must be on disk before the rest (new inodes, moved
  directories and fragments) is written in that order, which 18 rounds of
  power-cut testing, twice, showed to be consistent.
* ext2 stays, for reading other disks.

**Networking**
* Interfaces and a routing table; `ifconfig`, `route`, `getifaddrs`, the
  interface and route ioctls, `net.route` in sysctl; extra interfaces come up
  from `/etc/hostname.*`.  DHCP leaves an address set by hand alone.
* The Ethernet, ARP, IP, ICMP and TCP input paths were hardened against a
  hostile peer (and tested against one); `net.inet.icmp.bmcastecho`,
  `errppslimit` and `net.inet.tcp.rstppslimit` are the knobs.
* Random numbers come from ChaCha20.  `IP_HDRINCL` is defined.

**C library**
* Added: `sysctl`, `sethostname`, `getloadavg`, `getpass`, `vsyslog` and the BSD
  facilities, `err`/`warn`, `arc4random`, `getprogname`, `strtonum`,
  `getfsent`, `paths.h`, `sys/param.h`, `sys/queue.h`, `sys/cdefs.h`,
  `sys/sockio.h`, `net/if.h`, `net/route.h`.
* The legacy XSI interfaces (SUSv2–SUSv4) are declared by version:
  `gettimeofday`, `usleep`, `setitimer`, `setpgrp` and the XSI signal
  functions.  `ITIMER_VIRTUAL`, `ITIMER_PROF` and `RLIMIT_SIGPENDING` work.
* The C.UTF-8 tables follow glibc 2.44.

**Panics**
* A panic prints what failed, the process and a stack trace (resolve with
  `addr2line -e bsd.elf ADDRESS`), tells QEMU through pvpanic, does not sync
  the disks, and restarts the system after `kern.panic_reboot_wait_time`
  seconds (60 by default; -1 halts; root sets it).  See `sys/README.md` for
  QEMU's `-device pvpanic` and its `-action`.

**Defects fixed on the way**
* A failed `mmap` of typed memory freed the pool's pages (first into the free
  list, later a panic).
* `vsnprintf` in the kernel ignored the `#` flag (`%#x` printed nothing).
* `umount(2)` refused a mount nothing used.

## Tested

* Power cut: 18 rounds, twice, on an FFS root.
* os-test 5666 of 5816 and the Open POSIX Test Suite 1821 of 1858, on an FFS
  root; the failures are listed with their reasons in
  `share/doc/conformance.md`.
* `make regress`: libc 26 of 26, utilities 8361 passed, the rc tests 47 of 47,
  the kernel tests 16 of 17; `k_timer` is sensitive
  to the host's load and passes 13 of 13 on an idle host.

## Not in 1.0

* Real hardware is not a goal: the devices are those QEMU emulates.  One
  processor, up to 1 GiB of RAM.
* Of the interfaces of XSH, the typed memory objects were the ones the Linux
  kernel could not support; V86BSD's own kernel has them (`lib/libc/STATUS.md`).
* The FFS driver is new and has not had the years of use the BSDs' has.
* `brk`/`sbrk` are not in the C library yet; the Linux compatibility cannot
  be switched off at run time yet; the boot loader is QEMU's (`-kernel`).

## Upgrading from Shall 0.9.1

There is no upgrade path: make a new image (`make release`).  The root file
system is FFS2, not ext2; `boot.sh` runs `e2fsck` only for an ext2 image.
