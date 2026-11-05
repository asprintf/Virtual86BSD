# HOWTO: build Rogue inside Shall

Rogue 5.4.4 — the classic dungeon game, Toy, Arnold and Wichman's
1980s code as the Rogue Forge release of 1999 left it — builds inside a
Shall system with its own `configure` and makefile, on the ncurses of
`ports/ncurses`.  Four small changes to its sources are needed
(`posix.diff`); they are about the code's age, not about Shall: one of
them stops the build with any current ncurses, the others use functions
that POSIX.1-2024 removed.

## 1. Get the sources (on the host)

    sh ports/get.sh rogue

clones <https://github.com/Davidslv/rogue>, branch `master` (without git
it downloads the branch's tarball), into `root/usr/src/rogue-5.4.4`.
That repository keeps the 5.4.4 sources unchanged (its own fixes are on
the branch `modern-rogue`, which isn't used here).  With a copy you
already have: `sh ports/get.sh rogue path/to/rogue-master.zip`.

## 2. Build ncurses first (inside)

    sh /usr/src/shall/ports/ncurses/build.sh

## 3. Build and install Rogue (inside)

    sh /usr/src/shall/ports/rogue/build.sh

That applies `posix.diff` (once; the script sees when it is applied
already), then runs, with `CC=c17` and `CFLAGS='-O 1'`:

    ./configure --prefix=/usr/local \
        --enable-scorefile=/var/games/rogue.scr \
        --enable-lockfile=/var/games/rogue.lck
    make

and installs:

| Where | What |
|---|---|
| `/usr/local/bin/rogue` | the game |
| `/var/games/rogue.scr` | the score file (top ten), writable by everyone; `/var/games` is sticky, for the lock file |
| `/usr/local/share/man/rogue` | the manual page, for `man rogue` (and `man -k dungeon`; `/etc/profile` puts the directory in `MANPATH`) |
| `/usr/local/share/doc/rogue/rogue.doc` | *A Guide to the Dungeons of Doom*, plain text |

`-I` builds only.  The output goes to `build.log`.

## 4. Play

    rogue                 # a new game
    rogue -r              # restore the game saved with S (~/rogue.save)
    rogue -s              # the scores

`?` lists the commands, `/` says what a symbol on the screen is.  Move
with `h j k l y u b n` (`H J K L ...` runs), `Q` quits, `S` saves.  The
game needs a screen of at least 24×80.  `ROGUEOPTS` in the environment
sets options (`ROGUEOPTS='name=Joost,fruit=kiwi'`; see the guide, "Using the ROGUEOPTS variable").

On the QEMU console set `TERM` (and the size) first, see
`../README.md`, "Terminals".

## The changes (`posix.diff`)

- **`toascii()`**, twice (`fight.c`, `rooms.c`): POSIX.1-2008 marked it
  obsolescent and POSIX.1-2024 removed it, so Shall's `<ctype.h>` doesn't
  have it, and C99 (c17 too) doesn't allow calling an undeclared
  function.  It is `c & 0177`, written out.
- **`curscr->_cury`, `curscr->_curx`** (`main.c`, in `tstp()`, which
  redraws the screen after the game was stopped and continued): Rogue set
  curses' idea of the cursor position by writing into the `WINDOW`
  structure.  ncurses keeps that structure opaque, so this doesn't
  compile anywhere today.  The lines go: `mvcur()`, just before them,
  already tells ncurses where it leaves the cursor.
- **`getpass()`** (`mdport.c`, `md_getpass()`): also removed by
  POSIX.1-2024.  Rogue's fallback for systems without it is DOS code
  (`_getch()`); the diff adds one with POSIX `termios`: echo off, a line
  from `/dev/tty`, echo back on.  (Only the wizard mode asks for a
  password, and it is off in this build.)

## Tested

Built by `build.sh` inside a freshly bootstrapped Shall system, in a
chroot and on Shall's own kernel in QEMU, and played in a terminal
(xterm emulation): moving, the inventory, saving with `S` and
restoring with `-r`, quitting with gold (the score file then lists the
game: `rogue -s`), and being stopped with SIGTSTP and continued with `fg`
(the screen comes back with the cursor where it was).  ^Z inside the game
is a command key, not a stop (Rogue puts the terminal in raw mode), as
it always was.
