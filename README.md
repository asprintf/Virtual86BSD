# Virtual/86 BSD — the POSIX.1-2024 utilities and C library, on the FreeBSD kernel

Virtual/86 BSD (V86BSD) is a fork of Shall 0.9.1 for virtual machines.  It is
a system written from the standard: every utility of POSIX.1-2024 (XCU, with
the XSI option), a C library for x86-64 with all the interfaces of ISO C17
and POSIX.1-2024 (XSH), a C17 compiler, and the BSD-style system around
them (`init`, `getty`, `login`, `/etc/rc`, `rcctl`, `syslogd`, `ifconfig`, ...).
Its kernel and boot loader are FreeBSD's.

Version 1.0 (November 2026) ran on a kernel written for it.  Since
17 November 2026 it runs on **FreeBSD's kernel** (`releng/15.2`), FreeBSD's
boot loader and UFS2, configured for QEMU: the C library, the programs and
`/etc` were ported to FreeBSD's system call interface, and everything
(the kernel and loader too) is built from this tree by clang and lld.  The
old kernel, its loader and its FFS code are gone (commit `e791ee77` is
the last that has them).  `RELEASE-NOTES.md` says what that changed.

Shall, the system this one comes from, was made by Joost Orchidee Winter with Claude, Anthropic's AI model,
which wrote most of the code from the text of the standard (POSIX.1-2024,
The Open Group's edition), under his direction: he set the goals and made
the decisions, and the results were checked by the test suites described
below, by differential testing against other implementations, and by
external suites (the Open POSIX Test Suite, os-test, the POSIX Shell
Tests, the test suites of the ports).

## What is whose

* **FreeBSD's**: the kernel (`sys/`), the boot loader (`stand/`: BIOS and
  UEFI), the build support they need (`freebsd/`: `tools/build/make.py`,
  `share/mk`, bmake, `config`, `makefs`, `mkimg`) and the UFS tools
  (`newfs`, `fsck_ffs`, `tunefs`, `dumpfs`, `growfs`, in `freebsd/sbin`).
  Imported unchanged from `releng/15.2` by `distrib/import-freebsd.sh`
  (`freebsd/IMPORTED` has the commit), except for the six kernel patches in
  `distrib/freebsd/patches-sys/` and the configuration `sys/amd64/conf/V86`
  (CDDL parts, ZFS and DTrace are left out).  `share/doc/credits.md` has the
  licences.
* **V86BSD's own**: the C library (`lib/libc`, built for FreeBSD's ABI), all
  utilities, `init`, `/etc`, the compiler `c17`, and the scripts that build
  and test it all (`distrib/`).  The userland keeps source compatibility with
  the BSDs, not binary compatibility: a FreeBSD program does not run on
  V86BSD's C library (its dynamic linker is `/lib/libc.so`), and V86BSD's
  programs are not meant for a FreeBSD userland.

## Layout

    README.md       this file
    LICENSE         BSD 3-clause
    sys/            FreeBSD's kernel; sys/amd64/conf/V86 is the configuration
    stand/          FreeBSD's boot loader (gptboot, loader, loader.efi)
    freebsd/        the rest of FreeBSD that those need to build (IMPORTED
                    names the commit)
    include/        the C library's public headers (FreeBSD's constants and
                    structures where they meet the kernel)
    lib/libc/       the C library (README.md, STATUS.md; tools/)
    lib/libxcu/     code shared by the utilities (not installed)
    lib/libcurses/  X/Open Curses, with NetBSD's libterminfo
    bin/ sbin/      the essential utilities, one directory each
    libexec/        getty
    usr.bin/        the other utilities, one directory each (README.md: the
                    list, with tests); tl/ is the test language of their
                    tests (not POSIX, but installed too: /usr/bin/tl)
    usr.sbin/       syslogd, rcctl, makefs
    sbin/init/      init, the first process
    etc/            the configuration files of a V86BSD system
    share/doc/      conformance.md: the external test suites and their
                    results; credits.md: the code adapted from the BSDs and
                    imported from FreeBSD, with its licences; hier.md: the
                    file system; legacy.md: interfaces of older editions
    Makefile        make kernel, image, boot, regress ... (its head lists them)
    distrib/        the scripts behind all of that (below)
    regress/        the tests: regress/{bin,sbin,libexec,usr.bin,usr.sbin}/
                    <utility>, lib/libc (the C library), os-test (Sortix's
                    suites, with V86BSD's legacy suites), run_tests.sh
    ports/          third-party software for it: ncurses, Rogue, Lua, SQLite,
                    sqllogictest, Mbed TLS, curl (README.md; a HOWTO.md and a
                    build.sh for each)

The previous kernel's way of building (`bootstrap.sh`, `chroot.sh`,
`mkdisk.sh`, the stages of `distrib/build.sh`) is gone.  A chroot on the
Linux host is not possible any more, since the C library speaks FreeBSD's
system calls: the system runs in QEMU.

## Building and booting it

You need an x86-64 Linux host with **clang and lld** (they build the
kernel, the loader, the C library and every program; no gcc is used),
Python 3 (FreeBSD's `make.py` bootstraps bmake with it), `rsync`,
`/usr/bin/time`, and QEMU (`qemu-system-x86_64`) with KVM (the scripts
pass `-accel kvm`).  Nothing is installed outside the tree and needs root.

`make kernel image boot` does all of this (`make release` builds, `make boot`
boots the image in QEMU; the head of the `Makefile` lists the targets).
The steps are:

**1. The kernel and the boot loader** (`distrib/build-freebsd.sh`, which
makes a hard-link copy of `sys/`, `stand/` and `freebsd/` in `obj/freebsd`
that looks like a FreeBSD source root, and runs `make.py` there; about 5
minutes at `-j12`):

    W="WITHOUT_CDDL=yes WITHOUT_ZFS=yes WITHOUT_TESTS=yes"
    sh distrib/build-freebsd.sh -- -j12 $W WITH_DISK_IMAGE_TOOLS_BOOTSTRAP=yes KERNCONF=V86 kernel-toolchain
    sh distrib/build-freebsd.sh -- -j12 $W KERNCONF=V86 buildkernel
    sh distrib/build-freebsd.sh -- loader $W

(The `crtbegin.o` error at the end of the loader build is expected: it comes
from `stand/userboot/test`, which is not wanted.)  This gives the kernel
(`obj/freebsd/obj/.../amd64.amd64/sys/V86/kernel`), `pmbr`, `gptboot`,
`loader`, `loader.efi`, and the host tools `makefs` and `mkimg`.

**2. The system and a bootable image** (`distrib/boot-freebsd.sh`):

    sh distrib/boot-freebsd.sh        # builds everything, makes the image, boots it
    sh distrib/boot-freebsd.sh -n     # the same, but stops after the image
    sh distrib/boot-freebsd.sh -o dir # elsewhere than obj/boot

It builds the C library (`lib/libc`, static and `libc.so`) and every program
of `bin`, `sbin`, `libexec`, `usr.bin` and `usr.sbin` with clang, installs
them into `obj/boot/root` as hier(7) wants them, builds FreeBSD's UFS tools
against the same C library (`distrib/ufstools-freebsd.sh`), adds `etc/`, and
makes `obj/boot/v86bsd.img` with `distrib/mkimage-freebsd.sh`.  The image
is a GPT disk with an EFI system partition (`loader.efi` as
`EFI/BOOT/BOOTX64.EFI`), a `freebsd-boot` partition (`gptboot`; `pmbr` in the
protective MBR) and a UFS2 root with soft updates labelled `rootfs`, so it
boots by BIOS and by UEFI.  The same inputs give the same image byte for byte
(`SOURCE_DATE_EPOCH`, default 0).  `mkimage-freebsd.sh` takes `-o image`,
`-s MiB` (default 128), `-d dir` (a root directory copied over the default
files) and `-i init`; without a `/sbin/init` it puts a tiny static test
program in the image.

The programs are `-fPIE` and linked dynamically with `/lib/libc.so`
(`init` statically); `lib/libc/tools/libc-cc` is the compiler
wrapper that points clang at this tree's headers and library.

## Running it

The script boots the image in QEMU (q35, KVM, 2 CPUs, 512 MiB, a virtio disk
and virtio network card) with the serial console on your terminal.  The
loader (`console="comconsole,vidconsole"`) and the kernel use both the serial line
and, in a QEMU window (`-vga std`, no `-nographic`), the framebuffer, where the
text uses a font of the XNU console (`distrib/fonts`, converted by
`distrib/mkvtfont.c`; `credits.md`).  `init` runs `/etc/rc` and then a `getty`
on each line `/etc/ttys` switches on, `ttyu0` (serial) and `ttyv0` (screen);
log in as `root` (the password is `root` until `passwd` changes it).

* The kernel mounts `/` read-only from `/dev/gpt/rootfs` (the loader
  learns it from `/etc/fstab`) and starts `/sbin/init`, which opens
  `/dev/console` itself and leaves `/` read-only for `/etc/rc`.  `rc` runs
  `fsck -p` (FreeBSD's `fsck_ffs`), makes `/` writable with `mount -uw /`, then
  `mount -a`, `/etc/sysctl.conf`, `/etc/netstart`, the host name from
  `/etc/myname`, clears `/tmp`, writes `dmesg > /var/run/dmesg.boot`, and starts the daemons that
  `/etc/rc.conf` turns on, each a script in `/etc/rc.d` (`syslogd`, `cron`,
  `atd`; `rcctl enable atd`, `rcctl restart syslogd`, `rcctl ls on`).
* Disks have FreeBSD's names: `vtbd0` (virtio), `ada0` (AHCI), partitions
  `vtbd0p3`, labels in `/dev/gpt/`.  The network interface is `vtnet0`.  `/etc/netstart`
  configures it and `lo0` from `/etc/hostname.*`; for QEMU's user networking it is
  `/etc/hostname.vtnet0`, `/etc/mygate` and `/etc/resolv.conf`
  (10.0.2.15, the gateway 10.0.2.2, the name server 10.0.2.3).  **There is no DHCP client yet.**
* `ifconfig` shows and sets the interfaces (`ifconfig vtnet0 10.0.2.20/24`,
  `ifconfig vtnet0 mtu 1400 down`), `route` the routing table (`route show`,
  `route add default 10.0.2.2`), `sysctl` the kernel's variables (by name, through
  FreeBSD's MIB), `mount` and `umount` use `nmount(2)`, `ps` reads
  `kern.proc`, and `dmesg` the message buffer (`kern.msgbuf`).
* `shutdown -p now` (or `halt -p`, `reboot`) stops the system: init hangs up
  the sessions, runs `/etc/rc.shutdown`, ends every process and powers off
  after the disks are written.  `Ctrl-A x` leaves QEMU at once; the next
  boot finds the disk not clean and `fsck -p` repairs it.

`make boot` (`distrib/qemu-freebsd.sh [-g] [-e] [-i] [-n] [-m MiB] [-s CPUS]
[-p HOSTPORT:PORT] [IMAGE]`) boots an image that is already made: `-g` adds a
window with the VGA screen and the keyboard, `-e` runs without KVM, `-i` makes
the disk an AHCI disk (`ada0`), `-n` leaves out the network, `-p 8080:80`
forwards port 8080 of the host to port 80 of the system.  The C library's and
the utilities' tests run in the same kind of image (below).

## Testing

All tests run inside the system, on FreeBSD's kernel in QEMU with KVM, one
test at a time with a time limit.  The tests are built on the host by clang
against this C library; there is no compiler in the image yet (`c17` is not
built for it, see "Where things stand").

* The C library: `sh regress/lib/libc/vmrun.sh [-d] [-s smp] [-n] [TEST...]`
  (`-d`: dynamic, `-s`: processors).  47 of 47 pass, static, dynamic and
  with `-s 4`.  Most of its tests compare with glibc on the host
  (differential tests).
* os-test (Sortix's, 5863 tests in 15 suites: headers, namespace, every
  function at a basic level, fnmatch, regex, limits, I/O, processes, signals,
  pseudoterminals, stdio, UDP, Unix sockets): `sh lib/libc/tools/gate.sh ost WORKDIR`.
  5745 pass (glibc on Linux 5228 of an older snapshot), all of the header and namespace tests
  among them.  Every one of the other 118 is explained in
  `share/doc/conformance.md` (UDP's open questions, last-ulp long double results,
  readings of the standard).
* The Open POSIX Test Suite (LTP): `sh lib/libc/tools/gate.sh pts WORKDIR`.
  1805 of the 1876 that compile pass (glibc on Linux passes 1693 of its
  1848; V86BSD's old kernel passed 1812 of 1848).  Most of the 71 others have a
  cause in the kernel or the test that `conformance.md` names (no
  `SCHED_SPORADIC`, the scheduler's own `SCHED_OTHER` priorities, timers on
  CPU-time clocks, ...); about 27 have no proven cause yet.
* The utilities: `sh distrib/utilsuite-freebsd.sh [UTILITY...]` runs
  `regress/<dir>/<utility>/test_<utility>.sh` for every utility as root in
  QEMU, on programs built by `boot-freebsd.sh -n`.  The tests use only what
  POSIX specifies.  In the last full run 6484 tests passed and 556 failed
  in 137 utilities; about 300 of the failures need a compiler in the image
  (c17, yacc, lex), the others are programs that still read Linux's `/proc`
  (`fuser`, `ipcs`), expectations that were written for the old kernel
  (`sysctl`, `mount`, `umount`, `fsck`) and some that have not been looked
  at.  On the old kernel all 8403 passed.
* The POSIX Shell Tests (yash's test suite, 10,979 cases) against V86BSD's
  `sh`: 10,940 pass and 8 fail (yash 0 failures, dash 1,271, bash as `sh`
  1,634); the 8 are things POSIX.1-2024 leaves unspecified and one place
  where the 2024 text differs from the other shells (`bin/sh/README.md`).
  That run was made before the switch and has not been repeated.

`share/doc/conformance.md` has the suites side by side, with the
comparisons, the kernel patches the tests asked for, and what each result
that is not a pass comes from.  `make regress` runs the C library's tests,
`regress/etc` and the utilities'; `make gate-ost` and `make gate-pts` the two
big suites.

## Where things stand

All 155 utilities of POSIX.1-2024 exist (16 of them as built-ins of sh) and
1224 of the 1227 interfaces of XSH are in the C library (`lib/libc/STATUS.md`,
`usr.bin/README.md`); the other three are the typed memory objects, an
option FreeBSD does not offer.  `kill` can also be run through exec
(`env kill`), as POSIX requires of it alone among the intrinsic utilities.
Multi-character collating elements (`[.ch.]`) work in regular expressions
and patterns.

The system boots (BIOS and UEFI; virtio disk and AHCI; one to four
processors; serial and framebuffer console), has a multi-user login, the
daemons of `rc.d`, UFS2 with FreeBSD's `newfs` and `fsck_ffs`, and the
network of QEMU's user networking.  What the switch has not finished:

* **No compiler in the image.**  `c17` is in the tree, but it is not built
  for FreeBSD's ABI yet (it runs on the host's Linux only), so the system
  cannot compile anything itself, rebuild itself (`/usr/src` is not in the
  image) or run the tests that compile code.
* **The ports** (`ports/`) were built and tested on the old kernel and
  have not been tried on the FreeBSD one.
* **No DHCP client**; `/etc/netstart` says so for a `dhcp` line.

The to-do items are kept in the author's notes, not in the tree.

## License

BSD 3-clause; see `LICENSE`.  The tree holds V86BSD's own sources (those of
Shall, and the changes of the fork) and FreeBSD's `sys/`, `stand/` and
`freebsd/`, which keep their own copyright and licence notices (mostly the
BSD licences of the Regents of the University of California and of The
FreeBSD Project; `share/doc/credits.md`), and some code adapted from
OpenBSD and NetBSD with its notices.  The CDDL parts of FreeBSD (ZFS,
DTrace) are not in the tree.
Data a system needs is copied from the host when it is built (time zones,
terminfo, locale sources and charmaps, certificate authorities) and keeps its
own licenses; the ports are fetched by `ports/get.sh` and keep theirs (only
V86BSD's small diffs to them are here); the Open POSIX Test Suite (GPL) and
the POSIX Shell Tests (GPL) are used from outside the tree.  os-test (ISC) is
in the tree, in `regress/os-test`, under its own licence
(`regress/os-test/LICENSE`).
