# posixutilities — utilities of POSIX.1-2024

Implementations of the utilities of POSIX.1-2024 (IEEE Std 1003.1-2024,
The Open Group Base Specifications Issue 8, volume XCU), written from
the standard.  Each is in C11 with POSIX interfaces only, in its own
directory, with its own tests.  The aim is the full suite; 139 utilities
are done, in 125 directories (plus the built-ins of the shell), c17, the
C compiler, among them (see [Status](#status)).

    make              # build all of them
    make test         # build, then run every test suite (sh run_tests.sh)
    make difftest     # compare with other implementations, where installed
    make install      # into /usr/local/bin; PREFIX= and DESTDIR= as usual
    make clean

One utility: `cd sed && make test`, or `sh run_tests.sh sed ls`.

In V86BSD, these utilities and the C library in ../libc make a system that
compiles itself: ../bootstrap.sh builds it, from a host's C compiler
through two stages of its own, in a directory that ../chroot.sh enters
(see ../README.md).

## The utilities

| directory | utility | lines | tests |
|-----------|---------|------:|------:|
| [ar](ar/) | create and maintain library archives | 657 | 56 |
| [asa](asa/) | interpret carriage-control characters | 95 | 21 |
| [at](at/) | execute commands at a later time (with batch; and atd, its daemon) | 1235 | 88 |
| [awk](awk/) | pattern scanning and processing language | 5210 | 331 |
| [basename](basename/) | return non-directory portion of a pathname | 66 | 18 |
| [bc](bc/) | arbitrary-precision arithmetic language | 2986 | 12 |
| [c17](c17/) | compile standard C programs (with its own assembler and link editor) | 17936 | 302 |
| [cal](cal/) | print a calendar | 256 | 33 |
| [cat](cat/) | concatenate and print files | 100 | 13 |
| [cflow](cflow/) | generate a C-language flowgraph | 831 | 33 |
| [chgrp](chgrp/) | change the file group ownership | 296 | 15 |
| [chmod](chmod/) | change the file modes | 404 | 94 |
| [chown](chown/) | change the file ownership | 317 | 24 |
| [cksum](cksum/) | write file checksums and sizes | 97 | 10 |
| [cmp](cmp/) | compare two files | 162 | 35 |
| [comm](comm/) | select or reject lines common to two files | 134 | 19 |
| [compress](compress/) | compress and expand data (with uncompress and zcat) | 1741 | 123 |
| [cp](cp/) | copy files | 700 | 77 |
| [crontab](crontab/) | schedule periodic background work (and crond, its daemon) | 982 | 71 |
| [csplit](csplit/) | split files based on context | 323 | 53 |
| [ctags](ctags/) | create a tags file | 326 | 37 |
| [cut](cut/) | cut out selected fields of each line of a file | 400 | 39 |
| [cxref](cxref/) | generate a C-language program cross-reference table | 388 | 31 |
| [date](date/) | write the date and time | 274 | 58 |
| [dd](dd/) | convert and copy a file | 683 | 114 |
| [df](df/) | report free disk space | 319 | 32 |
| [diff](diff/) | compare two files | 1069 | 63 |
| [dirname](dirname/) | return the directory portion of a pathname | 66 | 16 |
| [du](du/) | estimate file space usage | 250 | 50 |
| [echo](echo/) | write arguments to standard output | 73 | 12 |
| [ed](ed/) | edit text | 2201 | 99 |
| [env](env/) | set the environment for command invocation | 198 | 29 |
| [expand](expand/) | convert tabs to spaces | 195 | 18 |
| [expr](expr/) | evaluate arguments as an expression | 329 | 52 |
| [false](false/) | return false value | 13 | 2 |
| [file](file/) | determine file type | 1162 | 109 |
| [find](find/) | find files | 957 | 100 |
| [fold](fold/) | filter for folding lines | 238 | 24 |
| [fuser](fuser/) | list process IDs of all processes that have one or more files open | 265 | 28 |
| [gencat](gencat/) | generate a formatted message catalog | 596 | 42 |
| [getconf](getconf/) | get configuration values | 1126 | 40 |
| [gettext](gettext/) | retrieve text string from messages object (with ngettext) | 677 | 82 |
| [grep](grep/) | search files for a pattern | 693 | 57 |
| [head](head/) | copy the first part of files | 146 | 16 |
| [iconv](iconv/) | codeset conversion | 659 | 47 |
| [id](id/) | return user identity | 210 | 38 |
| [ipcrm](ipcrm/) | remove an XSI message queue, semaphore set, or shared memory segment identifier | 104 | 36 |
| [ipcs](ipcs/) | report XSI interprocess communication facilities status | 350 | 40 |
| [join](join/) | relational database operator | 407 | 22 |
| [lex](lex/) | generate programs for lexical tasks (with libl.a) | 2772 | 11 |
| [link](link/) | call the link() function | 41 | 11 |
| [ln](ln/) | link files | 192 | 29 |
| [locale](locale/) | get locale-specific information | 577 | 40 |
| [localedef](localedef/) | define locale environment | 8261 | 54 |
| [logger](logger/) | log messages | 258 | 27 |
| [logname](logname/) | return the user's login name | 37 | 7 |
| [lp](lp/) | send files to a printer | 749 | 55 |
| [ls](ls/) | list directory contents | 982 | 61 |
| [m4](m4/) | macro processor | 1537 | 102 |
| [mailx](mailx/) | process messages | 4810 | 27 |
| [make](make/) | maintain, update, and regenerate files | 3597 | 14 |
| [man](man/) | display system documentation | 232 | 193 |
| [mesg](mesg/) | permit or deny messages | 73 | 20 |
| [mkdir](mkdir/) | make directories | 349 | 35 |
| [mkfifo](mkfifo/) | make FIFO special files | 194 | 19 |
| [more](more/) | display files on a page-by-page basis | 1716 | 91 |
| [msgfmt](msgfmt/) | create messages objects from portable messages object source files | 568 | 43 |
| [mv](mv/) | move files | 632 | 61 |
| [newgrp](newgrp/) | change to a new group | 252 | 26 |
| [nice](nice/) | invoke a utility with an altered nice value | 158 | 27 |
| [nl](nl/) | line numbering filter | 262 | 32 |
| [nm](nm/) | write the name list of an object file | 295 | 29 |
| [nohup](nohup/) | invoke a utility immune to hangups | 210 | 38 |
| [od](od/) | dump files in various formats | 618 | 65 |
| [paste](paste/) | merge corresponding or subsequent lines of files | 213 | 22 |
| [patch](patch/) | apply changes to files | 1572 | 70 |
| [pathchk](pathchk/) | check pathnames | 191 | 28 |
| [pax](pax/) | portable archive interchange | 3968 | 19 |
| [pr](pr/) | print files | 811 | 58 |
| [printf](printf/) | write formatted output | 462 | 71 |
| [ps](ps/) | report process status | 777 | 88 |
| [pwd](pwd/) | return working directory name | 103 | 12 |
| [readlink](readlink/) | display the contents of a symbolic link | 68 | 13 |
| [realpath](realpath/) | resolve a pathname | 187 | 22 |
| [renice](renice/) | set nice values of running processes | 173 | 26 |
| [rm](rm/) | remove directory entries | 424 | 78 |
| [rmdir](rmdir/) | remove directories | 83 | 22 |
| [sccs](sccs/) | the Source Code Control System: admin, delta, get, prs, rmdel, sact, sccs, unget, val, what | 4789 | 218 |
| [sed](sed/) | stream editor | 1399 | 123 |
| [sh](sh/) | shell, the standard command language interpreter | 11968 | 406 |
| [sleep](sleep/) | suspend execution for an interval | 72 | 14 |
| [sort](sort/) | sort, merge, or sequence check text files | 888 | 80 |
| [split](split/) | split a file into pieces | 224 | 35 |
| [strings](strings/) | find printable strings in files | 178 | 29 |
| [strip](strip/) | remove unnecessary information from strippable files | 635 | 29 |
| [stty](stty/) | set the options for a terminal | 612 | 124 |
| [tabs](tabs/) | set terminal tabs | 239 | 47 |
| [tail](tail/) | copy the last part of a file | 359 | 28 |
| [talk](talk/) | talk to another user | 923 | 26 |
| [tee](tee/) | duplicate standard input | 107 | 12 |
| [test](test/) | evaluate expression (with `[`) | 276 | 217 |
| [time](time/) | time a simple command | 194 | 27 |
| [timeout](timeout/) | execute a utility with a time limit | 464 | 70 |
| [touch](touch/) | change file access and modification times | 304 | 55 |
| [tput](tput/) | change terminal characteristics | 252 | 58 |
| [tr](tr/) | translate characters | 929 | 65 |
| [true](true/) | return true value | 13 | 2 |
| [tsort](tsort/) | topological sort | 263 | 20 |
| [tty](tty/) | return user's terminal name | 46 | 13 |
| [uname](uname/) | return system name | 65 | 16 |
| [unexpand](unexpand/) | convert spaces to tabs | 255 | 21 |
| [uniq](uniq/) | report or filter out repeated lines in a file | 164 | 19 |
| [unlink](unlink/) | call the unlink() function | 48 | 9 |
| [uucp](uucp/) | system-to-system copy (with uucico) | 212 | 40 |
| [uudecode](uudecode/) | decode a binary file | 291 | 31 |
| [uuencode](uuencode/) | encode a binary file | 138 | 31 |
| [uustat](uustat/) | uucp status enquiry and job control | 184 | 19 |
| [uux](uux/) | remote command execution | 385 | 34 |
| [vi](vi/) | ex and vi, text editors (one program; vi and view are links) | 12177 | 485 |
| [wc](wc/) | word, line, and byte or character count | 178 | 18 |
| [who](who/) | display who is on the system | 314 | 52 |
| [write](write/) | write to another user | 298 | 45 |
| [xargs](xargs/) | construct argument lists and invoke utility | 630 | 70 |
| [xgettext](xgettext/) | extract gettext call strings from C-language source files | 891 | 33 |
| [yacc](yacc/) | yet another compiler compiler (with liby.a) | 3693 | 20 |

Beyond POSIX, two more programs are a part of the system:

| directory | program | lines | tests |
|-----------|---------|------:|------:|
| [tic](tic/) | the terminfo compiler of libcurses (X/Open Curses), NetBSD's (see ../share/doc/credits.md) | 776 | 17 |
| [tl](tl/) | tl, the test language: a scripting language in the style of C, with POSIX and pseudo-terminals as its library | 6725 | 98 |

tl is the language the tests of the other utilities are written in where
the shell is not enough (below), installed in `/usr/bin` so that tests
like them can be written for anything else on the system; `man tl` and
`tl/tl.md` describe it.  Neither is counted in the totals below.

Lines: C, yacc and lex sources, without generated files and tests; the
sources in `lib/` (9254 lines) are counted once, in the total only
(141,617 in all).  Tests: what `make test` counts, 7048 in all; a count is of cases
or of case files, as each suite reports them.  Each README.md describes
what is implemented, the choices made where the standard leaves room, and
how it was tested, including the differential testing against other
implementations.

## Requirements

* A C compiler and a POSIX system.  The Makefiles do not name the
  compiler: `CC` is make's own, as POSIX specifies it: `c17` for a make
  of POSIX.1-2024 (this collection's make among them), `c99` for one of
  POSIX.1-2008/2017 (GNU make with `.POSIX:`, which calls gcc
  `-std=c99`); `make CC=...`, or `CC` in the environment, names another.
  The sources are C11 and also build as C99.  The collection builds with any POSIX make,
  including its own: `cd make && cc -O2 -o make *.c`, then `make/make`
  at the top.  This collection's c17 builds it too (with the C library
  of a Linux distribution, glibc, or with musl: see c17/README.md).
* The tests: a POSIX shell, the POSIX utilities and a C compiler (for tl,
  the test language, and the small helpers in `testlib/`); nothing else.
* `make difftest`: the reference implementations named in each README.md
  (GNU awk, bc, date, dd, diff, ed, find, id, m4, make, od, patch, pr and
  sort; glibc's getconf and iconv; gzip; Christos Zoulas's file; ncurses
  tput and tabs; procps ps; psmisc fuser; flex; bison and byacc; dash,
  bash, yash, mksh and busybox (also its cal); nvi and vim; GNU tar,
  bsdtar, cpio; CSSC; GNU df, du and who; util-linux mesg; GNU gettext's
  gettext, ngettext, msgfmt and xgettext; glibc's gencat, locale and
  localedef; GNU ar, nm, strip and cflow; Universal Ctags; gcc and the
  GNU assembler, and optionally c-testsuite and Csmith, for c17).

The tests use only what POSIX specifies: shell scripts are POSIX sh (no
bash), and the utilities they call are used with POSIX options only (no
`seq`, `mktemp`, `stat`, `readlink -f`, `sed -i` or `head -c`, for
example).  Where POSIX has no way to do something, `testlib/` has a small
C program for it: `fstat.c` prints a file's status (the conversions of
GNU `stat -c`, from lstat()), `ltouch.c` sets the times of a symbolic
link itself, and `testlib/ipc.c` makes the XSI IPC objects of ipcs and
ipcrm.

What a shell script cannot do well (drive a program on a pseudo-terminal
and read its screen, compare with a reference implementation case by
case, generate random programs, read binary formats) is written in tl,
a small language in the style of C made for these tests, and now a
program of the system in its own right ([tl/](tl/), installed as
`/usr/bin/tl`): one interpreter, `tl/tl.c` (C99 and POSIX, about 6700
lines), which each test script compiles for itself from these sources
(so the tests run on any host, and test the tl beside them, not one
installed earlier).  It has
dynamic values (strings of bytes, lists, maps), POSIX regular
expressions, the interfaces of POSIX as its library (processes, signals,
descriptors, user IDs, time) and pseudo-terminals with a terminal
emulator (xterm and ANSI sequences) for the interactive utilities.
`tl/tl.md` describes it.  In `testlib/`, the libraries of the tests
written in it: `ptyrun.tl` runs a
command with some of its descriptors on a pseudo-terminal, typing
`$PTYRUN_INPUT` on it when that is set (for tty, nohup, stty, ps and the
questions of compress and patch); `expect.tl` holds sessions in the
manner of expect (ed, bc, mailx); `cases.tl` the runner of regression
suites of case scripts with frozen output (make, mailx, pax, lex, yacc);
`mkterminfo.tl` writes the terminfo entries of tput, tabs and talk;
`mkelf.tl` writes ELF objects of any class and byte order for ar, nm and
strip; and
`tl.sh` runs a tl script for the `difftest` targets.

POSIX names no UTF-8 locale.  The tests that need one take
`$UTF8_LOCALE`, else C.UTF-8, else any UTF-8 locale `locale -a` lists;
with none, they are skipped (and not counted), and the rest run in the C
locale.  Files like `/dev/full` are used only where they exist.  The
tests of localedef that use the locales it makes (in the binary format
of the GNU C library) run only with that library.

## Conventions

Every utility directory has the same parts:

| file | contents |
|------|----------|
| the sources | `name.c` for a single-file utility; several modules and a header otherwise |
| `Makefile` | targets `all` (the default), `test`, `difftest`, `install`, `clean` |
| `README.md` | the sections Build and test, What is implemented, Choices where the standard leaves room, Testing, Files |
| `test_name.sh` | the tests: `make test`, or `sh test_name.sh` from anywhere |
| `tests/` | for the larger utilities: the test cases, expected results and test programs |

Makefiles are POSIX makefiles (`.POSIX:`) of one pattern: `CC`, `CFLAGS`
(default `-O2`), `LDFLAGS`, `PREFIX`, `BINDIR` (and `DESTDIR`) can be set
on the command line, and `STDFLAGS`, empty, more options for the
compiler: with gcc or clang `STDFLAGS="-std=c11 -Wall -Wextra"` gives the
warnings the sources are kept free of.  Each source defines its own feature-test macros
(`_POSIX_C_SOURCE`, `_XOPEN_SOURCE`) before any include, or includes the
project header that does; nothing depends on the makefile for them.

The sources are written for POSIX.1-2024: `_POSIX_C_SOURCE 202405L`, and
`_XOPEN_SOURCE 800` where they need XSI; none defines `_DEFAULT_SOURCE`,
`_GNU_SOURCE` or `_BSD_SOURCE`.  `lib/port.h` gives the C libraries of
POSIX.1-2008 (glibc, musl) what they lack of it: tcgetwinsize() and
tcsetwinsize() (through ioctl()), the tm_gmtoff and tm_zone members of
struct tm, and the declarations of setresuid() and setresgid().  With a
C library whose `<unistd.h>` says `_POSIX_VERSION 202405L` it supplies
nothing, so the same sources build with glibc, with musl and with a C
library of POSIX.1-2024, by gcc or by c17.  It also has `NORETURN` and
`PRINTF`, which are GNU attributes where the compiler has them.

Beyond POSIX, only where it has no way to do the thing at all: major()
and minor() of `<sys/sysmacros.h>` (ls, pax), setgroups() (atd, crond,
newgrp, and tl for the tests), the kernel's `/proc` (ps, fuser, ipcs) and
its table of mounted file systems (df).

Options are read with getopt() as POSIX specifies it: scanning stops at
the first operand, so `cat f -u` reads a file named `-u` (with glibc this
holds because the sources define `_POSIX_C_SOURCE` and not `_GNU_SOURCE`;
every test script of a utility that uses getopt() checks it).  env, find,
xargs and sort read their options by hand: the first three because the
utility's or expression's arguments follow, sort because it may take
options after operands (it is excepted from Guideline 9, as are lex,
yacc and make).  m4 does too, as its options may also come after
operands, and pr, for +page and the options with optional arguments (-e,
-i, -n, -s); dd has operands only.  nice, time and timeout use getopt(),
which stops at the utility operand: what follows it is the utility's.

at and crontab come with the daemons that run their jobs, atd and crond
(POSIX specifies none), installed into `$(PREFIX)/sbin`; so does uucp
with uucico, the other side of uucp and uux and the runner of their
queued jobs.  For users other
than root, at, crontab and newgrp have to be installed set-user-ID root
(`make install` does not do that; see their README.md).

Sources that serve several utilities are in `lib/`, once; the Makefiles
of the utilities compile them from there (`-I../lib`), so a utility's
directory builds with `lib/` beside it:

| file | contents | used by |
|------|----------|---------|
| `cpp.c`, `cpp.h` | a C tokenizer and preprocessor | c17, cflow, ctags, cxref |
| `cparse.c`, `cparse.h` | a forgiving C parser | cflow, ctags, cxref |
| `elf.c`, `elf.h` | ELF relocatable files and their symbols | ar, nm, strip, cflow |
| `arch.c`, `arch.h` | archives | ar, nm, strip |
| `cesc.c`, `cesc.h` | C escape sequences in string literals | gettext, msgfmt, xgettext |
| `plural.c`, `plural.h` | plural-form expressions | gettext, msgfmt |
| `po.c`, `po.h` | portable object (.po) files | msgfmt, xgettext |
| `terminfo.c`, `terminfo.h` | compiled terminfo entries | tabs, tput, talk |
| `utx.c`, `utx.h` | the user accounting database | lp, talk, who, write |
| `uucomm.c`, `uucomm.h` | the UUCP system | uucp, uux, uustat |
| `uucico.c` | uucico, the other side of uucp and uux | uucp, uux |
| `port.h` | POSIX.1-2024 on older C libraries; `NORETURN`, `PRINTF` | c17, cflow, ctags, cxref, ls, mailx, make, more, pax, sed, stty, talk, tput, vi; tl |

Each source file starts with a comment naming the file and what it holds
(`cut.c - cut: cut out selected fields of each line of a file`), followed
by the choices it makes where the standard leaves room.

Every test script ends with one line, `name: N passed, M failed`, and exits
0 only if nothing failed; `run_tests.sh` adds them up.  The suites of the
larger utilities each end with `N passed, M failed` too.  Frozen expected
results are recorded with the suite's `--freeze` (or `--update`) option.

Generated parsers and scanners (`*.tab.c`, `*.tab.h` and the lex output
of awk, bc and sh) are included, made with this collection's own yacc and
lex, so building needs only a C compiler.  They are newer than their
grammars; to regenerate them, delete them and run make (`YACC=`, `LEX=`).

Sanitizer builds go through `CC`:

    ASAN_OPTIONS=detect_leaks=0 \
    CC="cc -fsanitize=address,undefined -g" sh run_tests.sh

## Status

Done: the 139 utilities of the 125 directories above (ex with vi, [ with
test, batch with at, uncompress and zcat with compress, the ten of SCCS
in sccs, ngettext with gettext), and in the shell all the special
built-ins (break, colon, continue, dot, eval, exec, exit, export,
readonly, return, set, shift, times, trap, unset) and the intrinsic
utilities (alias, bg, cd, command, fc, fg, getopts, hash, jobs, kill,
read, type, ulimit, umask, unalias, wait).  `kill` is also installed as
a program (sh/kill.sh, which runs the built-in), so that `env kill`,
`xargs kill` and `find -exec kill` work, as POSIX.1-2024 (XCU 1.6)
requires of it alone among the intrinsic utilities.  The shell's pattern
matching (case, pathname expansion, `${x#pattern}`) is the C library's
fnmatch(), so bracket expressions follow the locale (classes, equivalence
classes, collating symbols).  With PATH unset, commands are searched for
in the standard path (`getconf PATH`).

c17 compiles, assembles and link-edits by itself, so the collection
needs nothing outside it but a C library and a kernel.  Built with c17
against glibc, the collection passes its tests exactly as when built
with gcc; built with c17 against musl, as when built with gcc against
musl (the differences from glibc are those of the C library: utmpx,
locales, iconv, the messages of regcomp(), the `REG_STARTEND` extension
that ed and sed use for lines with NUL bytes, the format of `%a`).

[REVIEW.md](REVIEW.md) records the review of the smaller utilities
against the standard (October 2026) and what it changed.
