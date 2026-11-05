# HOWTO: build ncurses inside Shall

ncurses 6.6 builds unchanged inside a Shall system: its own `configure`
(a 30 000-line autoconf script) runs on Shall's sh, sed, awk, grep and
expr; its makefiles run on Shall's make; c17 compiles and links, and ar
makes the libraries.  The tic it builds compiles the whole terminfo
database (2899 terminal types) into the same files as the host's tic.

## 1. Get the sources (on the host)

    sh ports/get.sh ncurses

downloads <https://ftp.gnu.org/gnu/ncurses/ncurses-6.6.tar.gz> and
unpacks it into `root/usr/src/ncurses-6.6`.  With a copy you already
have: `sh ports/get.sh ncurses path/to/ncurses-6.6.tar.gz`.

## 2. Build and install (inside)

    sh /usr/src/shall/ports/ncurses/build.sh

That runs:

    ./configure --prefix=/usr/local \
        --without-cxx --without-cxx-binding --without-ada \
        --without-shared --without-debug --without-manpages --with-tests \
        --enable-overwrite \
        --with-terminfo-dirs=/usr/share/terminfo \
        --with-default-terminfo-dir=/usr/share/terminfo
    make
    make install.libs install.includes install.data

with `CC=c17` and `CFLAGS='-O 1'`, then installs some of ncurses'
programs, makes the usual links to the libraries and compiles a small
check program with `-lcurses`.  The output goes to `build.log`.  Options:
`-n` keeps the terminfo database the system has (the host's, copied by
`bootstrap.sh`) instead of installing ncurses' own; `-T` leaves out the
test programs.

## What you get

| Where | What |
|---|---|
| `/usr/local/lib` | `libncursesw.a`, `libpanelw.a`, `libmenuw.a`, `libformw.a`, and as links to them `libncurses.a`, `libcurses.a`, `libpanel.a`, `libmenu.a`, `libform.a` |
| `/usr/local/include` | `curses.h`, `ncurses.h`, `term.h`, `termcap.h`, `unctrl.h`, `panel.h`, `menu.h`, `form.h`, `eti.h`, ... |
| `/usr/local/bin` | `tic`, `infocmp`, `toe`, `tset`, `clear`, and `captoinfo`, `infotocap`, `reset` (links), `ncursesw6-config` |
| `/usr/local/bin/ncurses/` | all of ncurses' test and demo programs, 86 of them (`-T`: none) |
| `/usr/local/bin` | some of those demos as commands: `worm`, `rain`, `xmas`, `blue`, `firework`, `hanoi`, `knight`, `tclock`, `gdc`, `bs` |
| `/usr/share/terminfo` | the terminfo database, compiled from ncurses' `terminfo.src` |

So a program builds with plain `c17 -o prog prog.c -lcurses` (or
`-lncursesw`, `-lpanel -lncurses`, ...): c17's profile for Shall's C
library searches `/usr/local/include` and `/usr/local/lib` after
`/usr/include` and `/usr/lib`.  `ncursesw6-config --cflags --libs` answers configure scripts
that ask.

Try the demos (`q` or Ctrl-C leaves most of them):

    worm   rain   xmas   blue   firework   hanoi   knight   tclock   gdc   bs
    /usr/local/bin/ncurses/ncurses    # a menu of tests: keys, attributes, colours, ACS, panels, forms
    ls /usr/local/bin/ncurses         # the others: lrtest, dots, railroad, newdemo, ...

## Choices

- **Wide characters.** The libraries are ncurses' wide-character ones
  (`ncursesw`, the default of 6.6), so programs work in UTF-8 locales
  (`LC_ALL=C.UTF-8`) as well as in single-byte ones.  The plain names
  (`-lncurses`, `-lcurses`) are links to them, as on most Linux systems.
- **`--enable-overwrite`**: the headers go to `/usr/local/include`
  itself, as `curses.h` and `term.h` usually are; without it, under any
  prefix but `/usr`, ncurses puts them in `include/ncursesw/`, where
  programs that `#include <curses.h>` don't find them.
- **Static only.** No `libncursesw.so`: the libraries are linked into each
  program.  ncurses' configure makes shared libraries with the options
  of the compilers it knows (gcc's `-fPIC` and `-shared` on Linux);
  c17 does both with `-G`, which configure doesn't know, so
  `--with-shared` wasn't tried.
- **Not ncurses' `tput` and `tabs`.** Shall has its own, the POSIX
  utilities; ncurses' versions would replace them.  ncurses builds them
  (in `progs/`), so they can be tried there.
- **No manual pages.** ncurses' pages are troff; Shall's `man` shows
  plain-text pages, and Shall has no troff.
- **No C++ and Ada bindings**: Shall has no compilers for those.

## What configure says that looks wrong but isn't

- `Warning: creating insecure temp directory` from `config.guess`: it
  looks for `mktemp`, which isn't a POSIX utility, and falls back to
  `mkdir`.  Harmless.
- `checking if SIGWINCH is defined... no`: it looks in `<sys/signal.h>`,
  a non-standard header.  The library uses `<signal.h>` and handles
  SIGWINCH all the same (`checking if you want SIGWINCH handler... yes`).
- `configure: WARNING: _XOPEN_SOURCE feature test macro appears to be
  predefined`: configure decided not to add `-D_XOPEN_SOURCE` (on a
  Linux whose C library isn't glibc it sees no need), and warns that the
  wide-character code then relies on the headers showing the X/Open
  names anyway.  Shall's do: without feature macros they show all of
  POSIX and XSI.
- `checking whether we are using the GNU C compiler... no`, and no gcc
  options: right, it's c17.

## Tested

ncurses 6.6 (20251230), built inside a freshly bootstrapped Shall system
in a chroot and on Shall's own kernel in QEMU:

- configure, make and install as above: no failures, no compiler
  warnings.
- The tic built here compiled all of `terminfo.src`; the host's
  infocmp (ncurses 6.4) shows every one of the 2899 entries the same as
  from the host's own tic.  The infocmp built here prints each entry of
  the host's terminfo database exactly as the host's infocmp does.
- In a terminal (xterm emulation): the test menu `test/ncurses` (keys,
  ACS line drawing, wide-character ACS in C.UTF-8), and Rogue
  (`ports/rogue`).
