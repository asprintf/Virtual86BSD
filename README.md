# Shall — the POSIX.1-2024 utilities, C library and kernel, compiled by themselves

Shall is a system written from the standard: every utility of
POSIX.1-2024 (XCU, with the XSI option), a C17 compiler among them, a
C library for x86-64 with all the interfaces of ISO C17 and POSIX.1-2024
(XSH), and a kernel that runs it (and Linux programs: it speaks Linux's
system call interface).  It needs nothing else to build itself: c17
compiles, assembles and links -- the kernel too; make, ar, yacc, lex and
sh do the rest.  On a Linux host it makes a small system you can chroot
into, or boot in QEMU with its own kernel.

This is version 0.9.1 (November 2026; `VERSION`), a preview: complete in
what POSIX asks of the interfaces and utilities, but young as a kernel
(see "Where things stand").  0.9.1 fixes what the external test suites
showed after 0.9 (`share/doc/conformance.md`).  1.0 is to add mkext2, fsck and
login inside the system.

Shall was made by Joost Orchidee Winter with Claude, Anthropic's AI model,
which wrote most of the code from the text of the standard (POSIX.1-2024,
The Open Group's edition), under his direction: he set the goals and made
the decisions, and the results were checked by the test suites described
below, by differential testing against other implementations, and by
external suites (the Open POSIX Test Suite, os-test, the POSIX Shell
Tests, the test suites of the ports).

## Layout

    README.md       this file
    LICENSE         BSD 3-clause
    bootstrap.sh    builds a Shall system from scratch (read its header)
    chroot.sh       enters it (with or without root privileges)
    mkdisk.sh       makes a bootable disk image of it (shall.img)
    boot.sh         boots that image with Shall's kernel in QEMU
    include/        the C library's public headers
    lib/libc/       the C library (README.md, STATUS.md; tools/)
    lib/libxcu/     code shared by the utilities (not installed)
    bin/ sbin/      the essential utilities, one directory each (as in OpenBSD)
    usr.bin/        the other utilities, one directory each (README.md: the
                    list, with tests); tl/ is the test language of their
                    tests (not POSIX, but installed too: /usr/bin/tl)
    sys/            the kernel (README.md); sys/arch/amd64/compile/GENERIC
                    builds it
    sbin/init/      init, the first process
    etc/            the configuration files of a Shall system
    share/doc/      conformance.md: the external test suites and their results
    distrib/        scripts that make the disk image
    regress/        the tests: regress/{bin,sbin,usr.bin}/<utility>, lib/libc
                    (the C library), sys (the kernel), run_tests.sh
    ports/          third-party software for it: ncurses, Rogue, Lua, SQLite,
                    sqllogictest, Mbed TLS, curl
                    (README.md; a HOWTO.md and a build.sh for each)

## Building a system

You need x86-64 Linux with a C compiler, make and ar (on Ubuntu:
`build-essential`).  Then:

    sh bootstrap.sh            # builds root/ (a few minutes), as yourself
    sh chroot.sh               # enters root/: Shall's sh, with /usr/bin first

or, to run it on its own kernel instead of the host's (see "Booting it"):

    sh mkdisk.sh               # root/ as a disk image, shall.img
    sh boot.sh                 # boots it in QEMU; the shell comes up here

`chroot.sh` needs no privileges: as an ordinary user it enters through a
user namespace (`unshare -r`, in which you are root and have your own
`/proc`), or through bubblewrap or PRoot when those are installed;
with sudo it uses chroot itself.  Ubuntu 23.10 and later may refuse
user namespaces to ordinary users (see the head of `chroot.sh`).

`bootstrap.sh` works in stages, each from a fresh copy of the sources:

1. **stage 0** — the host's compiler builds just five tools of this
   package: c17, make, ar, yacc and lex.
2. **stage 1** — those tools build the C library (`libc.a`, and
   `libc.so`, which is also its own dynamic linker) and then every
   utility against it (c17, make and ar too).  No program of the host
   compiles anything from here on.
3. **stage 2** — the utilities of stage 1, running on Shall's own C
   library, build everything again.
4. **check** — stage 1 and stage 2 are compared file by file: the c17
   that the host's compiler made and the c17 that c17 made must produce
   identical programs.
5. **stage 3** — the utilities of stage 2 build the utilities for
   `root/`, dynamically linked with `/usr/lib/libc.so` and stripped:
   some 4.7 MB for the 143 programs (49 MB linked statically).  (The
   programs of stages 1 and 2 run on the host, so they are linked
   statically; `-n` puts those in `root/` instead of stage 3's.)
6. **kernel** — c17 of stage 2 builds the kernel and `/sbin/init`:
   `root/boot/shall-kernel`.

The grammars of awk, bc and sh are generated afresh by Shall's yacc and
lex in every stage.

`root/` then gets `/etc` (from `etc`), `/tmp`, `/root`, `/bin`,
`/lib` and `/sbin` (links into `/usr`), empty `/dev`, `/proc` and `/sys` (which
`chroot.sh` mounts), these sources in `/usr/src/shall`, and the host's
time zone database in `/usr/share/zoneinfo` (if it has one; `ZONEINFO=dir`
names another) and its terminfo database in `/usr/share/terminfo` (for
tput, more, vi; `TERMINFO=dirs` names others), its locale sources and
charmaps in `/usr/share/i18n` (for `localedef`, so that locales can be
made inside: `localedef -f UTF-8 -i nl_NL nl_NL.UTF-8`; `I18N=dir` names
another; compressed charmaps are uncompressed on the way), and its certificate
authorities in `/etc/ssl/certs/ca-certificates.crt` (for curl, see
`ports/`; `CABUNDLE=file` names another).  Inside, the
system rebuilds itself the same way, with its own c17 as the host's
compiler, and gives the same files:

    CC=c17 sh /usr/src/shall/bootstrap.sh /tmp/again

and so does the kernel (`cd /usr/src/shall/kernel; make`).  In an
interactive shell `/etc/profile` already sets `CC=c17`, so there `sh
/usr/src/shall/bootstrap.sh /tmp/again` is enough; scripts that do not
start from such a shell, such as `/etc/rc`, give `CC=c17` themselves.

Options: `-k` keeps the stages, `-w dir` puts them elsewhere, `-n`
leaves out stage 3.  `sh chroot.sh root command...` runs one command
inside.

Programs in the system are linked dynamically with `/usr/lib/libc.so`
(the C library, which is also the dynamic linker), and so are by default
the programs c17 makes there: `c17` finds the C library through its
profile `/usr/lib/c17/libc/shall`.  `c17 -B static` links a program
with `/usr/lib/libc.a` instead, and `c17 -G` makes shared libraries
(position-independent code; `dlopen` works).

Third-party software builds too, see `ports/README.md`, and goes to
`/usr/local`: ncurses 6.6 (through its own `configure`, unchanged) and
Rogue 5.4.4 on it, Lua 5.4 (it passes its own test suite), SQLite 3.54
(its C test programs and sqllogictest's 5.9 million queries pass), and
Mbed TLS 3.6 with curl 8.23, with which the system reaches the Internet,
in the chroot and on its own kernel in QEMU.  On the host, `sh
ports/get.sh all` puts the sources in `root/usr/src`; inside, `sh
/usr/src/shall/ports/all.sh` builds and installs them.

## Booting it

`bootstrap.sh` also builds Shall's kernel, `root/boot/shall-kernel`.
To run the system on it, the host needs QEMU (`qemu-system-x86_64`; on
Ubuntu the package `qemu-system-x86`) and e2fsprogs (`mke2fs`, which
makes the disk image); no root privileges:

    sh mkdisk.sh               # shall.img: root/ as an ext2 disk (1 GiB)
    sh boot.sh                 # boots it; the console is this terminal

The kernel starts `/sbin/init`, which runs `/etc/rc` if there is one and
then the shell on the console.  `exit` from that shell shuts the system
down (init writes everything back to the disk and powers off);
`Ctrl-A x` leaves QEMU at once (then the disk is checked, and repaired
where needed, with `e2fsck -p` the next time `boot.sh` starts it; it does
that every time).  What you change inside stays in
`shall.img` for the next boot; `sh mkdisk.sh` makes it afresh from
`root/` (`-s size` in MiB; operands name another root and image; for
another root, give `boot.sh` its kernel with `-k root/boot/shall-kernel`).

The disk is a virtio disk (`/dev/vda` inside); `-i` makes it a PC's IDE
disk (`/dev/sda`) instead, which is much slower.  `boot.sh` options: `-m 2G` the memory (default 1G), `-g` a window with
the VGA screen and the keyboard instead of this terminal, `-n` no network,
`-p 8080:80` forwards port 8080 of the host (127.0.0.1) to port 80 of the
system, `-k kernel` another kernel, `-e` no KVM, and after `--` more
arguments for QEMU.  It uses KVM when `/dev/kvm` is usable (much faster;
Shall has been tested under QEMU's emulation).

The network is QEMU's user networking: the system gets 10.0.2.15 by DHCP
(the host is 10.0.2.2, the name server 10.0.2.3), and connections out go
through the host.  See `sys/README.md` for the kernel and what it
offers.

## Testing

* The utilities: `sh regress/run_tests.sh` on the host; inside the
  system `cd /usr/src/shall && CC=c17 sh regress/run_tests.sh`.  The
  tests use only what POSIX specifies (no `yes`, `head -5`, `grep -o`,
  `cat -n` and the like), so all of some 6 700 pass inside the system as
  on the host.
* The C library: `cd libc && make check` — most of its tests are
  differential, comparing with glibc, so they need gcc and glibc (see
  `lib/libc/README.md`); `LIBC_LINK=dynamic` and `static-pie` run them
  linked those ways.
* The Open POSIX Test Suite (LTP, 1888 tests), built and run inside the
  system on its own kernel in QEMU: of the 1777 that compile (the other
  111 use interfaces POSIX.1-2024 removed, such as `gettimeofday`, or
  non-POSIX ones), 1741 pass; glibc on Linux passes 1693 of them.  None
  of the other 36 is a fault of Shall: 26 do not pass with glibc either
  (options not offered, indeterminate limits, "may fail" errors, defects
  of the tests); the others test undefined or implementation-defined
  behaviour, are defects of the tests, or (one) need more time than QEMU's
  emulation gives them (`lib/libc/STATUS.md`, stage 17).
* os-test (Sortix's, 5816 tests in 14 suites: headers, namespace, every
  function at a basic level, fnmatch, regex, limits, I/O, processes,
  signals, pseudoterminals, stdio, UDP), built inside the system and run
  on its own kernel in QEMU with `lib/libc/tools/ost/`: 5616 pass (glibc on
  Linux 5228), all of the header and namespace tests among them.  Of the
  other 200, 85 do not compile (83 call `usleep` or `gettimeofday`), 69
  have no known outcome yet (UDP's open questions), and the rest are
  QEMU's software x87, last-ulp long double results, test defects,
  os-test's "nonstandard" tests and a few readings of the standard where
  Shall does as glibc (`lib/libc/STATUS.md`, stage 18).
* The POSIX Shell Tests (yash's test suite, 10,979 cases) against Shall's
  `sh`: 10,940 pass and 8 fail (yash 0 failures, dash 1,271, bash as `sh`
  1,634); the 8 are things POSIX.1-2024 leaves unspecified and one place
  where the 2024 text differs from the other shells
  (`bin/sh/README.md`).

`share/doc/conformance.md` has all three suites side by side, with the
comparisons and what each result that is not a pass comes from.

## Where things stand

See `lib/libc/STATUS.md` and `usr.bin/README.md`.  In short: all 155
utilities of POSIX.1-2024 exist (16 of them as built-ins of sh), and
1224 of the 1227 interfaces of XSH; the other three are the typed memory
objects, an option Linux cannot support.  `kill` can also be run
through exec (`env kill`), as POSIX requires of it alone among the
intrinsic utilities.  Multi-character collating elements (`[.ch.]`)
work in regular expressions and patterns.  On Shall's own kernel the
utilities' tests pass but for a few (`sys/README.md`), and the
typed memory objects exist too.

## License

BSD 3-clause; see `LICENSE`.  The package holds only Shall's own sources.
Data a system needs is copied from the host when `bootstrap.sh` builds
it (time zones, terminfo, locale sources and charmaps, certificate
authorities) and keeps its own licenses; the ports are fetched by
`ports/get.sh` and keep theirs (only Shall's small diffs to them are
here); the Open POSIX Test Suite (GPL), os-test (ISC) and the POSIX
Shell Tests (GPL) are used from outside the tree.
