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

## What there is

| header | interfaces | last edition |
|---|---|---|
| `<arpa/inet.h>` | `inet_lnaof`, `inet_makeaddr`, `inet_netof`, `inet_network` | SUSv2 |
| `<ctype.h>` | `isascii`, `toascii`, `_toupper`, `_tolower` | SUSv4 |
| `<errno.h>` | `ENODATA`, `ENOSR`, `ENOSTR`, `ETIME` | SUSv4 |
| `<fnmatch.h>`, `<glob.h>`, `<regex.h>`, `<wordexp.h>` | `FNM_NOSYS`, `GLOB_NOSYS`, `REG_ENOSYS`, `WRDE_NOSYS` (never returned) | SUSv3 |
| `<langinfo.h>` | `YESSTR`, `NOSTR` | SUSv2 |
| `<libgen.h>` | `regcmp`, `regex`, `__loc1` | SUSv2 |
| `<limits.h>` | `DBL_DIG`, `DBL_MAX`, `FLT_DIG`, `FLT_MAX`; `NL_NMAX`; `PASS_MAX`, `TMP_MAX` | SUSv4; SUSv3; SUSv2 |
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
| `<unistd.h>` | `setpgrp`, `_POSIX2_FORT_DEV`; `usleep`, `ualarm`, `getwd`, `vfork`, `_XOPEN_LEGACY`, the `XBS5` names; `brk`, `sbrk`, `chroot`, `ctermid`, `cuserid`, `getdtablesize`, `getpagesize`, `getpass`, `_POSIX2_C_VERSION`, `_XOPEN_XCU_VERSION`, `_XOPEN_XPG2`-`4`, `_SC_PASS_MAX` | SUSv4; SUSv3; SUSv2 |
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
