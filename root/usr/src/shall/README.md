# Shall — the POSIX.1-2024 utilities and C library, compiled by themselves

Shall is a userland written from the standard: every utility of
POSIX.1-2024 (XCU, with the XSI option), a C17 compiler among them, and
a C library for x86-64 Linux with all the interfaces of ISO C17 and
POSIX.1-2024 (XSH).  It needs nothing else to build itself: c17
compiles, assembles and links; make, ar, yacc, lex and sh do the rest.
On top of a Linux kernel it makes a small system you can chroot into.

## Layout

    README.md       this file
    bootstrap.sh    builds a Shall system from scratch (read its header)
    chroot.sh       enters it (with or without root privileges)
    libc/           the C library: include/, src/, tests/, tools/ (README.md, STATUS.md)
    utilities/      the utilities, one directory each; lib/ and testlib/
                    shared by them (README.md: the list, with tests)
    system/etc/     the configuration files of a Shall system

## Building a system

You need x86-64 Linux with a C compiler, make and ar (on Ubuntu:
`build-essential`).  Then:

    sh bootstrap.sh            # builds root/ (a few minutes), as yourself
    sh chroot.sh               # enters root/: Shall's sh, with /usr/bin first

`chroot.sh` needs no privileges: as an ordinary user it enters through a
user namespace (`unshare -r`, in which you are root and have your own
`/proc`), or through bubblewrap or PRoot when those are installed;
with sudo it uses chroot itself.  Ubuntu 23.10 and later may refuse
user namespaces to ordinary users (see the head of `chroot.sh`).

`bootstrap.sh` works in stages, each from a fresh copy of the sources:

1. **stage 0** — the host's compiler builds just five tools of this
   package: c17, make, ar, yacc and lex.
2. **stage 1** — those tools build the C library and then every utility
   against it (c17, make and ar too).  No program of the host compiles
   anything from here on.
3. **stage 2** — the utilities of stage 1, running on Shall's own C
   library, build everything again into `root/`.

Finally stage 1 and stage 2 are compared file by file: the c17 that the
host's compiler made and the c17 that c17 made must produce identical
programs.  The grammars of awk, bc and sh are generated afresh by
Shall's yacc and lex in both stages.

`root/` then gets `/etc` (from `system/etc`), `/tmp`, `/root`, `/bin`
and `/lib` (links into `/usr`), empty `/dev`, `/proc` and `/sys` (which
`chroot.sh` mounts), and these sources in `/usr/src/shall`.  Inside, the
system rebuilds itself the same way, with its own c17 as the host's
compiler:

    CC=c17 sh /usr/src/shall/bootstrap.sh /tmp/again

Options: `-s` also builds `libc.so` (the shared C library, which is its
own dynamic linker); it needs gcc and binutils' ld, since c17 makes no
shared objects yet.  `-k` keeps the stages, `-w dir` puts them
elsewhere.  `sh chroot.sh root command...` runs one command inside.

Programs in the system are linked statically against `/usr/lib/libc.a`;
`c17` finds the C library through its profile `/usr/lib/c17/libc/shall`.

## Testing

* The utilities: `cd utilities && sh run_tests.sh` on the host; inside the
  system `cd /usr/src/shall/utilities && CC=c17 sh run_tests.sh`.  Inside,
  about 65 of some 6 700 tests fail, mostly because the tests call
  programs that are not POSIX utilities (`yes`, `cc`, `cat -n`, mail
  delivery) or need a terminal; the write tests wait for a terminal and
  must be interrupted.
* The C library: `cd libc && make check` — most of its tests are
  differential, comparing with glibc, so they need gcc and glibc (see
  `libc/README.md`); `LIBC_LINK=dynamic` and `static-pie` run them
  linked those ways.

## Where things stand

See `libc/STATUS.md` and `utilities/README.md`.  In short: all 155
utilities of POSIX.1-2024 exist (16 of them as built-ins of sh), and all
interfaces of the C library.  Known gaps: the regular built-ins cannot
yet be run through exec (e.g. `env kill`); strptime's `%E` and `%O`;
multi-character collating elements (`[.ch.]`); libc.so needs gcc.  The
kernel is Linux's.
