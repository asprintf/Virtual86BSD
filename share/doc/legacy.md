# Interfaces of older editions of the standard

Later editions removed interfaces that earlier ones had: Issue 6 (SUSv3,
2001) dropped SUSv2's LEGACY interfaces, Issue 7 (2008) and Issue 8
(POSIX.1-2024) more. V86BSD keeps them, back to the Single UNIX
Specification, Version 2, for programs written for those editions. The
lists were made from the texts: the NAME sections of every SUSv2 function
page and the contents of every SUSv2 header page, compared with
POSIX.1-2024 (`regress/os-test/legacy/*.api`).

None is visible by accident: each is declared only under the feature-test
macros of the editions that had it (`include/bits/features.h`), or with
no macro, `_DEFAULT_SOURCE`, `_GNU_SOURCE`, `_BSD_SOURCE`; never in a
strict ISO C compilation.

| window | macro | editions |
|---|---|---|
| `__XSI_OB` | `_XOPEN_SOURCE` 500, 600, 700 | SUSv2 to SUSv4 (XSI) |
| `__XSI_OB6` | `_XOPEN_SOURCE` 500, 600 | SUSv2, SUSv3 (XSI) |
| `__XSI_OB5` | `_XOPEN_SOURCE` 500 | SUSv2 alone |
| `__POSIX_RANGE(lo, hi)` | `_POSIX_C_SOURCE` (or what `_XOPEN_SOURCE` implies) in [lo, hi) | POSIX itself, not only XSI |
| (in `<unistd.h>`) | `_XOPEN_SOURCE` 600, 700 | SUSv3, SUSv4 (XSI): `_XOPEN_STREAMS` |

## What there is

| header | interfaces | last edition |
|---|---|---|
| `<arpa/inet.h>` | `inet_lnaof`, `inet_makeaddr`, `inet_netof`, `inet_network` | SUSv2 |
| `<ctype.h>` | `isascii`, `toascii`, `_toupper`, `_tolower` | SUSv4 |
| `<errno.h>` | `ENODATA`, `ENOSR`, `ENOSTR`, `ETIME` | SUSv4 |
| `<fnmatch.h>`, `<glob.h>`, `<regex.h>`, `<wordexp.h>` | `FNM_NOSYS`, `GLOB_NOSYS`, `REG_ENOSYS`, `WRDE_NOSYS` (never returned) | SUSv3 |
| `<langinfo.h>` | `YESSTR`, `NOSTR` | SUSv2 |
| `<libgen.h>` | `regcmp`, `regex`, `__loc1` | SUSv2 |
| `<limits.h>` | `DBL_DIG`, `DBL_MAX`, `FLT_DIG`, `FLT_MAX`, the `_POSIX_TRACE_*_MAX` minimums; `NL_NMAX`; `PASS_MAX`, `TMP_MAX` | SUSv4; SUSv3; SUSv2 |
| `<math.h>` | `MAXFLOAT`; `scalb`; `gamma` | SUSv4; SUSv3; SUSv2 |
| `<netdb.h>` | `gethostbyname`, `gethostbyaddr`, `h_errno`, `HOST_NOT_FOUND`, `NO_DATA`, `NO_RECOVERY`, `TRY_AGAIN` | SUSv3 |
| `<pthread.h>` | `pthread_attr_getstackaddr`, `pthread_attr_setstackaddr`; `pthread_getconcurrency`, `pthread_setconcurrency` | SUSv3; SUSv4 |
| `<re_comp.h>` | `re_comp`, `re_exec` | SUSv2 |
| `<regexp.h>` | `compile`, `step`, `advance`, `loc1`, `loc2`, `locs` | SUSv2 |
| `<setjmp.h>` | `_setjmp`, `_longjmp` | SUSv4 |
| `<signal.h>` | `sighold`, `sigrelse`, `sigignore`, `sigpause`, `sigset`, `siginterrupt`, `SIG_HOLD`, `SIGPOLL`'s `POLL_*`; `bsd_signal`; `sigstack`, `struct sigstack` | SUSv4; SUSv3; SUSv2 |
| `<stdio.h>` | `tempnam`, `P_tmpdir`; `cuserid`, `L_cuserid`, `getw`, `putw`, `getopt` and its variables | SUSv4; SUSv2 |
| `<stdlib.h>` | `rand_r`; `ecvt`, `fcvt`, `gcvt`, `mktemp`; `ttyslot`, `valloc` | SUSv4; SUSv3; SUSv2 |
| `<strings.h>` | `bcmp`, `bcopy`, `bzero`, `index`, `rindex` | SUSv3 |
| `<sys/time.h>` | `gettimeofday`, `getitimer`, `setitimer`, `struct itimerval`, `ITIMER_*` | SUSv4 |
| `<sys/timeb.h>` | `ftime`, `struct timeb` | SUSv3 |
| `<sys/types.h>` | `useconds_t` | SUSv3 |
| `<sys/wait.h>` | `wait3` | SUSv2 |
| `<termios.h>` | `IUCLC`, `OLCUC`, `XCASE` (the flags; the terminal driver does not act on them yet) | SUSv2 |
| `<time.h>` | `asctime_r`, `ctime_r`; `CLK_TCK` | SUSv4; SUSv2 |
| `<ucontext.h>` | `getcontext`, `setcontext`, `makecontext`, `swapcontext` | SUSv3 |
| `<ulimit.h>` | `ulimit`, `UL_GETFSIZE`, `UL_SETFSIZE` | SUSv4 |
| `<unistd.h>` | `setpgrp`, `_POSIX2_FORT_DEV`, the `POSIX_V6` environments (`_POSIX_V6_*`, `_SC_V6_*`, `_CS_POSIX_V6_*`, `_CS_V6_ENV`), the unsupported options `_POSIX_TRACE*`, `_POSIX2_PBS*`, `_XOPEN_STREAMS` (-1) and their `_SC_` names; `usleep`, `ualarm`, `getwd`, `vfork`, `_XOPEN_LEGACY`, the `XBS5` names; `brk`, `sbrk`, `chroot`, `ctermid`, `cuserid`, `getdtablesize`, `getpagesize`, `getpass`, `_POSIX2_C_VERSION`, `_XOPEN_XCU_VERSION`, `_XOPEN_XPG2`-`4`, `_SC_PASS_MAX` | SUSv4; SUSv3; SUSv2 |
| `<utime.h>` | `utime`, `struct utimbuf` | SUSv4 |
| `<wchar.h>` | the `isw*` and `tow*` functions, `wctype`, `wctype_t`; `wcswcs` | SUSv4; SUSv3 |

## Choices the texts leave open

* `ecvt`, `fcvt`, `gcvt`: digits from `printf`, at most 17; `ecvt` gives
  exactly `ndigit` digits when rounding carries (glibc gives one more).
* `sigstack`: `ss_onstack` 0 takes the stack away, as SUSv2 says (4.3BSD
  read it as "on the stack now"); `ss_sp` is the top of a `SIGSTKSZ` stack.
* `pthread_attr_setstackaddr`: the lowest address, as FreeBSD and NetBSD
  (glibc takes the highest); `pthread_attr_setstacksize` keeps it.
* `cuserid`: the effective user, as glibc.
* `scalb` with an `n` that is not a whole number: `EDOM`, as glibc.
* `setcontext` returns into the context through `rt_sigreturn`, so a
  signal handler's context resumes exactly.
* `<regexp.h>` and `re_comp`: `^` and `$` also match at a newline;
  `re_comp`'s `.` and `[^...]` do not match one. Bracket expressions are
  those of the locale, a byte at a time.
* `regcmp`, `regex`: translated to an extended regular expression.
* `gethostbyname`, `gethostbyaddr`: on `getaddrinfo` and `getnameinfo`.
* `gets()` is not there: C11 removed it.

## X/Open Curses

SUSv2 includes X/Open Curses, Issue 4, Version 2 (XCURSES): `<curses.h>`,
`<term.h>`, `<unctrl.h>`, the curses and terminfo functions, and the
terminfo source format. `lib/libcurses` is NetBSD's libcurses and
libterminfo made to follow those texts (`share/doc/credits.md` lists
what was changed); `-l curses` links it (`-l terminfo` and `-l termcap`
too), and `tic` (`usr.bin/tic`) compiles terminal descriptions. ncurses
stays a port (`ports/ncurses`, in `/usr/local`).

* The terminal database is NetBSD's format, a cdb file:
  `/usr/share/misc/terminfo.cdb`, compiled at build time from
  `share/terminfo/terminfo` (ncurses' terminfo.src as NetBSD has it), so
  it does not depend on the build host. `TERMINFO` names another database
  file (or holds a description), `TERMINFO_DIRS` a list of them, and
  `~/.terminfo.cdb` is read first; ansi, dumb, vt100, vt220 and xterm are
  also built into the library. `/usr/share/terminfo` (the host's
  ncurses-format tree) is still what `tput`, `tabs` and `talk` read.
* Where X/Open Curses, Issue 7 (2009) changed a declaration (`const`
  formats and names, `va_list` for `va_list *`, `COLOR_PAIR` and
  `PAIR_NUMBER` macros), the declaration is Issue 7's. Where Issue 7 kept
  SUSv2's or withdrew the interface, it is SUSv2's, without the `const`
  NetBSD and ncurses add: `setupterm`, `restartterm`, `tgetflag`,
  `tgetnum`, `tgetstr`, `tgoto`, `vwprintw`, `vwscanw`.
* `innstr`, `inchnstr`, `getnstr` (and their wide forms) store at most
  `n` bytes or elements, the terminating null included (ncurses stores
  `n` and the null).
* In the POSIX locale a terminal without `acsc` draws lines with the
  characters of the `<curses.h>` table (`+`, `-`, `|` ...); the library
  never sets the locale itself.
* `vid_attr` and `vid_puts` with an `opts` that is not null: `ERR`.
  `prefresh` and `pnoutrefresh` with a screen rectangle that is not on the
  screen: `ERR` (the pad's rectangle is cut to the pad).
* `scr_dump` writes the virtual screen as `putwin` writes a window;
  `scr_init` does nothing (`OK`) when the terminal has `rmcup` and
  `nrrmc`.
* `restartterm` changes the current terminal's description in place, so
  that the screens of curses keep pointing to it.
* After `start_color`, pair 0 is the terminal's own colours, taken to be
  white on black, as the text assumes a black background.

## Not done yet

* XTI (`<xti.h>`, the `t_*` functions of SUSv2's Networking Services) and
  STREAMS (`<stropts.h>`, `fattach`, `getmsg`, `putmsg`, `isastream` ...).
* `<varargs.h>`: needs the compiler.
* The terminal driver's `IUCLC`, `OLCUC` and `XCASE`.

## Not planned

* The utilities SUSv2 had and later editions removed (`cpio`, `tar`,
  `pack`, `pcat`, `unpack`, `egrep`, `fgrep`, `pg`, `col`, `sum`, `c89` ...).

## Tests

* `regress/os-test/legacy-include`: every declaration, compiled with
  `_XOPEN_SOURCE=500` (made from `regress/os-test/legacy/*.api`).
* `regress/os-test/legacy-basic`: what each function does.
* `regress/lib/libc/legacy.sh`: every interface under every feature-test
  macro, visible exactly in its window.
* `regress/os-test/xcurses-include`: every declaration of `<curses.h>`,
  `<term.h>` and `<unctrl.h>`, with `_XOPEN_SOURCE=500` and
  `_XOPEN_SOURCE_EXTENDED=1`, linked with `-l curses` (made from
  `regress/os-test/xcurses/*.api`).
* `regress/lib/libcurses`: what curses draws, on a pseudo-terminal of a
  test type that `vtscreen.c` emulates, and what the functions return.
