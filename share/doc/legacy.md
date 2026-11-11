# Interfaces of older editions of the standard

POSIX.1-2024 (Issue 8) removed interfaces that earlier editions had, and
Issue 7 (POSIX.1-2008) had removed others. V86BSD keeps a few of them for
programs written for those editions. They are never visible by accident:
each has a window of feature-test macros taken from the text of the
editions (Single UNIX Specification versions 2, 3 and 4, and POSIX.1-2024),
and none is declared with `_POSIX_C_SOURCE` alone or in a strict ISO C
compilation. `regress/lib/libc/legacy.sh` tests the windows (every
interface under every macro) and `regress/lib/libc/s_legacy.c` the
behaviour.

The editions: SUSv2 is `_XOPEN_SOURCE=500` (UNIX 98, POSIX.1-1996),
SUSv3 is 600 (POSIX.1-2001), SUSv4 is 700 (POSIX.1-2008), POSIX.1-2024 is
800. With no macro at all, or `_DEFAULT_SOURCE`, `_GNU_SOURCE`,
`_BSD_SOURCE`, everything is visible (`__EXT_VISIBLE`); the windows are
`__XSI_OB` and `__XSI_OB6` in `include/bits/features.h`.

| interface | header | SUSv2 | SUSv3 | SUSv4 | 2024 | visible with `_XOPEN_SOURCE` |
|---|---|---|---|---|---|---|
| `gettimeofday()` | `<sys/time.h>` | yes | XSI | OB XSI | removed | 500, 600, 700 |
| `getitimer()`, `setitimer()`, `struct itimerval`, `ITIMER_*` | `<sys/time.h>` | yes | XSI | OB XSI | removed | 500, 600, 700 |
| `sighold()`, `sigrelse()`, `sigignore()`, `sigpause()`, `sigset()`, `SIG_HOLD` | `<signal.h>` | yes | XSI | OB XSI | removed | 500, 600, 700 |
| `siginterrupt()` | `<signal.h>` | yes | XSI | OB XSI | removed | 500, 600, 700 |
| `setpgrp()` | `<unistd.h>` | yes | XSI | OB XSI | removed | 500, 600, 700 |
| `POLL_IN` ... `POLL_HUP` (SIGPOLL's codes; STREAMS option) | `<signal.h>` | yes | yes | OB XSR | removed | 500, 600, 700 |
| `usleep()` | `<unistd.h>` | yes | OB XSI | removed in Issue 7 | - | 500, 600 |

"OB" is the standard's mark for obsolescent. The table is made from the
texts: the `[XSI]` and `[OB XSI]` marks of each interface's page, and the
tables "Removed Functions and Symbols in Issue 7" and "Removed Functions
in Issue 8" of the rationale (XRAT B.3).

## What each does, from the text

* `gettimeofday(tp, tzp)`: the time as seconds and microseconds since the
  Epoch; a `tzp` that is not null has unspecified behaviour, so it is left
  alone. Implemented on `clock_gettime(CLOCK_REALTIME)`.
* `usleep(usec)`: SUSv3: the argument "shall be less than one million", and
  the function "may fail" with `EINVAL` if it is not. V86BSD does not fail:
  it sleeps the longer time. 0 has no effect. A caught signal ends the
  sleep: -1 and `EINTR`.
* `getitimer()`, `setitimer()`: the kernel's.
* `sigset(sig, disp)`: `SIG_HOLD` blocks `sig` and keeps the disposition;
  any other `disp` unblocks `sig`; a handler runs with `sig` blocked and
  the mask is restored after it. The value is `SIG_HOLD` if `sig` was
  blocked, else the old disposition. `SIG_ERR` and `EINVAL` for an illegal
  signal, or one that cannot be caught or ignored.
* `sighold()`, `sigrelse()`: block, unblock; `sigignore()`: `SIG_IGN`;
  `sigpause(sig)`: removes `sig` from the mask, waits for a signal and
  restores the mask; it returns -1 and `EINTR`. This is System V's
  `sigpause()`: 4.3BSD's took a mask, and is not what the standard says.
* `siginterrupt(sig, flag)`: as the standard shows it, on `sigaction()`
  and `SA_RESTART`.
* `setpgrp()`: makes the process a group leader (`setpgid(0, 0)`); no
  effect in a session leader; returns the process group ID; no errors.

## Not done, on purpose

* `gets()`: ISO C removed it in C11, and the compiler is C17.
* Everything else Issue 7 and Issue 8 removed is not there yet:
  `bcmp`, `bcopy`, `bzero`, `index`, `rindex`, `bsd_signal`, `ecvt`,
  `fcvt`, `gcvt`, `ftime`, `getcontext`, `makecontext`, `setcontext`,
  `swapcontext`, `gethostbyaddr`, `gethostbyname`, `h_errno`, `getwd`,
  `mktemp`, `pthread_attr_getstackaddr`, `pthread_attr_setstackaddr`,
  `scalb`, `ualarm`, `vfork`, `wcswcs`, `_longjmp`, `_setjmp`, `_tolower`,
  `_toupper`, `ftw`, `isascii`, `toascii`, `pthread_getconcurrency`,
  `pthread_setconcurrency`, `rand_r`, `tempnam`, `ulimit`, `utime`, and
  the STREAMS (`fattach`, `fdetach`, `getmsg`, `getpmsg`, `putmsg`,
  `putpmsg`, `isastream`) and tracing (`posix_trace_*`) options. An
  interface is added when a test suite or a port needs it, one at a time,
  with its window and its tests.
