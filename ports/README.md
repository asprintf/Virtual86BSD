# ports — third-party software for a V86BSD system

Each port is a directory with a `HOWTO.md` (what it takes, what you get,
what was changed and why) and a `build.sh` that does the work inside the
system; a port that needs changes to its sources carries them as a
diff, which its `build.sh` applies.  Everything is built by V86BSD's own
tools: c17, make, sh, sed, awk, ar.  No gcc, binutils, GNU make, bash,
Perl or Python.

| Port | Version | Changes to its sources | Built with | Tests inside V86BSD |
|---|---|---|---|---|
| [ncurses](ncurses/HOWTO.md) | 6.6 | none | its `configure` and makefiles | tic/infocmp agree with the host's on 2899 terminals |
| [rogue](rogue/HOWTO.md) | 5.4.4 | `posix.diff`: 4 small changes | its `configure` and makefile (needs ncurses) | played |
| [lua](lua/HOWTO.md) | 5.4.7 | none | c17 and ar | Lua's test suite |
| [sqlite](sqlite/HOWTO.md) | 3.54.1 | `posix-src.diff`: 6 small changes | its `configure` (autosetup) and makefiles | fuzzcheck, mptest, threadtest3, speedtest1 |
| [sqllogictest](sqllogictest/HOWTO.md) | 2026-10 | `posix.diff`: 1 line | c17 (needs sqlite) | 5.9 million queries, twice |
| [mbedtls](mbedtls/HOWTO.md) | 3.6.7 | `shall.diff`: no inline assembly, gettimeofday | c17 and ar | selftest, TLS 1.2/1.3, 139 unit suites |
| [curl](curl/HOWTO.md) | 8.23.0 | `shall.diff`: c17 in `<curl/system.h>` | its `configure` and makefiles (needs mbedtls) | HTTPS to localhost and the Internet |

V86BSD's C library is strictly POSIX.1-2024: interfaces the standard
removed (`gettimeofday`, `setitimer`, `ctime_r`, ...) are not there, also
not as extensions, so ports that still call them carry small diffs
(SQLite: `gettimeofday` three times; Mbed TLS: its timing module).  That
is a decision, not a gap: V86BSD is kept a minimal POSIX system.

## The way it goes

The system has no `gzip` and fetches nothing by itself until curl is
built, so the sources are fetched on the host, into the system's
`/usr/src`, by `get.sh`:

    sh ports/get.sh all                  # every port (git, curl or wget)
    sh ports/get.sh ncurses              # or one
    sh ports/get.sh ncurses ~/ncurses-6.6.tar.gz      # or a copy you have

`get.sh -r dir` names another system than `root/`; `-f` replaces sources
that are there already.  They land in `/usr/src/NAME-VERSION`, where the
build scripts look by default (each takes another directory as its
argument).

Then enter the system, `sh chroot.sh`, or boot it: `get.sh` writes into
`root/`, so make the disk image afterwards (`sh mkdisk.sh -s 4096` with
sqllogictest's 1.1 GB of tests; it makes the image afresh), and
`sh boot.sh`.  Inside:

    sh /usr/src/ports/all.sh       # all that are in /usr/src, in order; -t: their tests

or one at a time:

    sh /usr/src/ports/ncurses/build.sh
    sh /usr/src/ports/rogue/build.sh
    rogue

Each `build.sh` installs (unless `-I`), writes the output of configure
and make to `build.log` in the sources, and stops at the first failure,
naming it.  `-t` runs the port's tests.  The scripts are POSIX sh and can
be run again.

## Where they go: /usr/local

The ports install under `/usr/local` (`bin`, `lib`, `include`,
`share/man`, `share/doc`), apart from the system's own programs in `/usr`:

- `PATH` has `/usr/local/bin` first (`/etc/profile`, `chroot.sh`, init);
- c17 (its profile for V86BSD's C library, `/usr/lib/c17/libc/v86bsd`)
  searches `/usr/local/include` and `/usr/local/lib` after `/usr/include`
  and `/usr/lib`, so `c17 prog.c -lcurses` just works;
- the dynamic linker searches `/usr/local/lib` (for `libsqlite3.so`);
- `MANPATH` (`/etc/profile`) has `/usr/local/share/man` (plain-text
  pages, as V86BSD's `man` reads them: `man rogue`).

Two things stay in the system's places: ncurses' terminfo database in
`/usr/share/terminfo` (the system's `tput`, `more` and `vi` read it too)
and Rogue's score file in `/var/games`.

## Time taken

Measured on a 2-CPU cloud machine; KVM should come close to the chroot
column.

| | chroot | QEMU, emulation (TCG, `boot.sh -e`) |
|---|---|---|
| ncurses (with the terminfo database and the demos) | 1¾ min | 40 min |
| rogue | 8 s | 2¾ min |
| lua, with its tests | 5 s | 1¾ min |
| sqlite (source tree; `-t` adds 12 min in the chroot) | 40 s | 12 min |
| sqllogictest: the build / the whole run with `-j 2` | 5 s / 10½ min | |
| mbedtls, with `-t` (`-T`: 20 min more) | 40 s | |
| curl, with `-t` | 3 min | |
| all of them, `all.sh` (without `-t`; `-t`: 15 min in the chroot) | 8 min | 2½ hours |

## The Internet

With curl the system reaches the Internet, in the chroot through the
host's network and on V86BSD's kernel in QEMU through QEMU's user
networking (10.0.2.15 by DHCP; the host is 10.0.2.2): see
[curl/HOWTO.md](curl/HOWTO.md).  `bootstrap.sh` copies the host's
certificate authorities to `/etc/ssl/certs/ca-certificates.crt`.

## Terminals

ncurses finds a terminal's description by `TERM` in `/usr/share/terminfo`.
`ncurses/build.sh` installs ncurses' whole database there (2899 terminal
types; `-n` keeps the host's copy that `bootstrap.sh` made).

- In `chroot.sh`, `TERM` comes from the host and the size from its
  terminal, as usual.
- On the QEMU console (`boot.sh` without `-g`) the system can't know what
  terminal QEMU's output goes to: init sets `TERM=vt100` and the size is
  24×80.  In an xterm-like terminal (GNOME Terminal, Windows Terminal,
  iTerm, PuTTY...) set both for colour and the whole window:

      TERM=xterm-256color; export TERM
      stty rows 50 columns 160         # what your window has (on the host: stty size)

  Resizing the window later isn't seen (a serial line carries no size).
