# Status per header

Legend: **done** = complete per POSIX.1-2024 and tested; **partial** =
the header declares only what is implemented; **compiler** = supplied by
the compiler (gcc / c17), not by this library.

## Stage 1: foundation

| Header | State | Notes |
|---|---|---|
| `<assert.h>` | done | message written with one `write(2)` (stderr is unbuffered, so the effect is the same and works even with stdio locked) |
| `<ctype.h>` | done | the locale's classes for bytes 0x80-0xff (stage 5); `*_l` variants |
| `<endian.h>` | done | |
| `<errno.h>` | done | all Linux error numbers; per-thread `errno` |
| `<inttypes.h>` | done | `wcstoimax`, `wcstoumax` since stage 5 |
| `<iso646.h>` | done | |
| `<limits.h>` | done | indeterminate limits omitted as required (use `sysconf`) |
| `<stdalign.h>` | done | |
| `<stdbool.h>` | done | |
| `<stdint.h>` | done | |
| `<stdnoreturn.h>` | done | |
| `<string.h>` | done | Two-Way `strstr`; `strerror_r` is the POSIX (int) version |
| `<strings.h>` | done | |
| `<sys/types.h>` | done | pthread types sized as in the glibc ABI |
| `<stddef.h>`, `<stdarg.h>`, `<float.h>`, `<stdatomic.h>` | compiler | |

## Stage 2: system interfaces

| Header | State | Notes |
|---|---|---|
| `<fcntl.h>` | done | `O_CLOFORK`/`FD_CLOFORK`/`F_DUPFD_CLOFORK` native on Shall's kernel, emulated on Linux; `O_EXEC`/`O_SEARCH` = `O_PATH` |
| `<poll.h>` | done | |
| `<signal.h>` | done | signals 32–34 reserved for the library, `SIGRTMIN` 35; `sig2str`/`str2sig`; `psignal`/`psiginfo` write with one `writev(2)` (atomic, stderr unbuffered) |
| `<sys/mman.h>` | done; typed memory where the kernel has it (Shall's), else the functions fail as for no typed memory | `shm_open` in `/dev/shm`; `POSIX_MADV_DONTNEED` is a no-op (Linux's discards data) |
| `<sys/resource.h>` | done | |
| `<sys/select.h>` | done | |
| `<sys/stat.h>` | done | |
| `<sys/statvfs.h>` | done | |
| `<sys/sysmacros.h>` | extension | `major`, `minor`, `makedev` (used by ls and pax) |
| `<sys/time.h>` | done | |
| `<sys/times.h>` | done | |
| `<sys/uio.h>` | done | |
| `<sys/utsname.h>` | done | |
| `<sys/wait.h>` | done | |
| `<termios.h>` | done | incl. `tcgetwinsize`/`tcsetwinsize` |
| `<unistd.h>` | done except `crypt`, `encrypt` | `getlogin`/`getlogin_r` arrived in stage 4; `_POSIX_VERSION` 202405L |

Also implemented: `setgroups()` (to be declared in `<grp.h>`).

## Stage 3: time, stdio, stdlib

| Header | State | Notes |
|---|---|---|
| `<time.h>` | done | TZ strings and TZif v2+ files (`/etc/localtime`, `/usr/share/zoneinfo`, footer rule beyond the table); a valid POSIX TZ string wins over a file of the same name (glibc prefers the file); `tm_gmtoff`/`tm_zone`; `getdate` with `DATEMSK`; `SIGEV_THREAD` timers return `ENOTSUP` until `<pthread.h>` |
| `<stdio.h>` | done | buffered `FILE` with recursive per-file locks; exact `printf` of all floating types (round-half-even, `%a` included); correctly rounded `strtod`/`scanf`; positional arguments (`NL_ARGMAX` 32); `fmemopen`, `open_memstream`, `popen`, `getdelim`, `dprintf`, `asprintf` |
| `<stdlib.h>` | done except `setkey` | `setkey` comes with `crypt`; `random` matches glibc's sequences; `system` via `clone`/`execve` with SIGINT/SIGQUIT ignored and SIGCHLD blocked; `realpath` resolves component by component; `a64l` sign-extends (POSIX), glibc does not; `mkostemp`, `ptsname_r`, `qsort_r`, `reallocarray`, `secure_getenv` |

Locale: only the POSIX locale for now; `MB_CUR_MAX` is 1 and bytes
0x80–0xff map to the wide characters 0xdf80–0xdfff (as in musl), so every
byte string round-trips.

Known differences from glibc that the differential tests exclude (all
cases where this library follows POSIX or glibc is inconsistent): TZ
string precedence, DST rules before 1970 for TZ strings, `tzname` taken
from the current rules, `%C` for years below 1000, `strftime` `+` flag,
`mktime` after 2038 in some zones, the unseeded `rand48` state,
`strptime` with `%E`/`%O` in a locale without eras or alternative digits
(the plain conversions here, as POSIX says; glibc fails).

## Stage 4: directories, users, patterns, processes

| Header | State | Notes |
|---|---|---|
| `<dirent.h>` | done | `struct dirent`/`posix_dent` have the kernel's `linux_dirent64` layout, so `posix_getdents` is `getdents64`; `DT_MQ`/`DT_SEM`/`DT_SHM`/`DT_TMO` are never returned (no such file types on Linux); a removed directory reads as empty |
| `<pwd.h>`, `<grp.h>` | done | the files `/etc/passwd` and `/etc/group` only (no name-service switch); `setgroups` declared in `<grp.h>` only without feature-test macros (it is not POSIX) |
| `<utmpx.h>` | done | `struct utmpx` is POSIX (`ut_tv` a `struct timeval`, plus `ut_host` and `ut_exit` as glibc); the database is `/run/utmp` (or `/var/run/utmp`) in the traditional 384-byte Linux record format, converted on reading and writing; the first `pututxline` creates it (mode 0644) where there is none, as POSIX has it write for a process with the privileges (0.9.1); `RUN_LVL` only without feature-test macros |
| `<unistd.h>` | `getlogin`, `getlogin_r` added | the `USER_PROCESS` entry for the terminal on fd 0, 1 or 2; else the audit login uid |
| `<regex.h>` | done | own engine, see below; `REG_MINIMAL` and the `?` repetition modifier (new in POSIX.1-2024); `\?`, `\+`, `\|` in BREs act as in EREs; back-references also in EREs; `REG_STARTEND` extension; error messages as glibc's |
| `<fnmatch.h>` | done | incl. `FNM_CASEFOLD`/`FNM_IGNORECASE` |
| `<glob.h>` | done | |
| `<wordexp.h>` | done | the expansions are done by `/bin/sh` |
| `<ftw.h>` | done | `nftw` (POSIX.1-2024 dropped `ftw`); at most three descriptors at a time |
| `<spawn.h>` | done | incl. the `[PS]` scheduling attributes (`POSIX_SPAWN_SETSCHEDULER`, `POSIX_SPAWN_SETSCHEDPARAM`): applied in the child before the file actions; an error is returned as `sched_setscheduler`/`sched_setparam` would |
| `<setjmp.h>` | done | |
| `<libgen.h>` | done | |

**Regular expressions.** POSIX's rules for subexpressions are followed
exactly: the whole match is leftmost-longest, then each subpattern from
left to right is longest (shortest for a minimal repetition); a repeated
subexpression reports its last iteration, and subexpressions inside it
that did not take part in that iteration are -1. glibc departs from this
in many cases (e.g. `(a|ab)(c|bcd)(d*)` on `abcd`: POSIX gives
`(0,2)(2,3)(3,4)`, glibc `(0,1)(1,4)(4,4)`), and it misses some matches
with back-references to repeated groups (`\(b\)\{0,2\}\1` on `bbcb`).
The whole-match results agree with glibc (tested on 100 000 random
patterns). The matcher simulates a Thompson NFA (linear time) for the
match, then fixes the subexpressions top-down with forward and backward
simulations of each subpattern's piece of the program; with
back-references it backtracks, pruned by the same simulations.

## Stage 5: locales and wide characters

| Header | State | Notes |
|---|---|---|
| `<locale.h>` | done | see "Locales" below; `getlocalename_l` (new in POSIX.1-2024); per-thread locales with `uselocale` |
| `<langinfo.h>` | done | items numbered as in glibc (the index in its locale files); `ALTMON_x`, `ABALTMON_x` (new); `YESSTR`/`NOSTR` without feature-test macros |
| `<wchar.h>` | done | wide printf runs the narrow engine on the format converted to multibyte (widths, precisions and counts in characters); `swprintf` converts through UTF-8, so every wide character passes, as in glibc; wide scanf has an engine of its own (widths, `%c`, `%[` and `%n` in characters; numbers read by the narrow scanners); wide functions do not refuse a byte-oriented stream |
| `<wctype.h>` | done | the classes and maps of the locale file, or built-in Unicode data for C.UTF-8 |
| `<uchar.h>` | done | `mbrtoc16`/`c16rtomb` with surrogate pairs |
| `<iconv.h>` | done | UTF-8/16/32, UCS-2/4, WCHAR_T, ASCII, 35 single-byte codesets, EUC-JP (with JIS X 0212), SHIFT_JIS, ISO-2022-JP, EUC-CN, GBK, GB18030, EUC-KR, BIG5; `//TRANSLIT` (the locale's transliterations, else `?`), `//IGNORE`; `""` the locale's codeset |
| `<nl_types.h>` | done | catalogs in glibc's format (which the suite's gencat writes); `NLSPATH` |
| `<libintl.h>` | done | `gettext` family incl. the `_l` forms (new in POSIX.1-2024): `.mo` objects, `Plural-Forms`, `LANGUAGE`, `NLSPATH`, codeset conversion with iconv |
| `<monetary.h>` | done | `strfmon`, `strfmon_l` |
| `<inttypes.h>` | `wcstoimax`, `wcstoumax` added | |

Also locale-dependent now: `printf` (decimal point, `'` grouping flag),
`strtod`/`scanf` (radix character), `strftime`/`strptime` (names and
formats, `%Ob`/`%OB`), `isalpha` and friends for bytes 0x80-0xff and
`toupper`/`tolower` (single-byte locales; Turkish), `regex` and `fnmatch`
(multibyte characters in UTF-8 locales), `scanf` `%ls`/`%lc`.

**Locales.** Built in: `C`/`POSIX` and `C.UTF-8` (also `C.utf8`; Unicode
classes, case maps and widths generated from glibc's C.utf8 data by
`tools/mkuctype.c`). Others are directories in glibc's binary format --
what the `localedef` of the POSIX utilities (or glibc's) writes -- in
`$LOCPATH` or `/usr/lib/locale`, `en_US.UTF-8` also found as `en_US.utf8`.
Supported codesets: UTF-8, the single-byte codesets iconv knows, and
EUC-JP, SHIFT_JIS, EUC-KR, GB2312 (EUC-CN), GBK, GB18030 and BIG5 (stage 12).
A name with the UTF-8 codeset and no data is accepted as C.UTF-8 under that name.
The POSIX locale has all 256 bytes as characters (bytes 0x80-0xff are the
wide characters 0xdf80-0xdfff), as POSIX.1-2024 requires; glibc's does not
convert them.

Deliberate differences from glibc found by the tests: UTF-8 beyond
U+10FFFF is rejected; `fnmatch("??", "é")` does not match in UTF-8
(glibc: it does); bracket ranges like `[α-ω]` work in UTF-8 regular
expressions (glibc's C.utf8: an error); `strfmon` with `+` fails when both
signs are empty, as POSIX requires (in the POSIX locale); C.UTF-8 has no
`combining` class and no transliterations.

## Stage 18: os-test

[os-test](https://gitlab.com/sortix/os-test) (Sortix, ISC licensed,
outside the tree; a snapshot of 5 November 2026: 5816 tests in 14
suites) was built inside the system by c17 and run on Shall's kernel in
QEMU; the include and namespace suites only compile.  Its makefiles need
GNU or BSD make, so `tools/ost/` runs it with POSIX sh (`ost.sh`), with a
compiler command (`cc`) that turns gcc's options into c17's.  For
comparison it ran on the host as its makefiles do, with gcc 13 and glibc
2.39 on Linux 6.18.  Judged by os-test's own `misc/html` (expectations in
`<suite>.expect`, "none" where they are unknown):

| suite | Shall | glibc/Linux |
|---|---|---|
| basic | 1151/1187 | 1077/1187 |
| fnmatch | 61/67 | 49/67 |
| include | 3758/3758 | 3471/3758 |
| io | 55/55 | 53/55 |
| limits | 124/126 | 119/126 |
| malloc | 3/3 | 3/3 |
| namespace | 159/159 | 130/159 |
| paths | 48/48 | 48/48 |
| process | 24/24 | 21/24 |
| pty | 28/29 | 25/29 |
| regex | 93/96 | 83/96 |
| signal | 28/32 | 29/32 |
| stdio | 25/25 | 25/25 |
| udp | 59/207 | 95/207 |
| total | 5616/5816 | 5228/5816 |

The first run gave 5501.  It found, and this round fixed:

* In this library: the mathematical functions raised `FE_INVALID` for an
  infinite argument (`x - x != 0` as the test for "not finite", in some
  30 functions, now `__ld_nonfinite`; `roundl`'s `inf - inf`);
  `strtod("inf")` raised `FE_DIVBYZERO`, and so did an overflowing
  `strtod`, `fma`, `csqrt`, `clog` and `cproj` (now `FE_OVERFLOW` or
  none); `lrint`/`lround` of a NaN, an infinity or a value out of range
  set no `EDOM`, `logb(0)` no `ERANGE` (both required by POSIX; glibc
  sets neither); `bindtextdomain(NULL, ...)` set `errno`; the network
  databases, utmpx and syslog set `errno` when their file or `/dev/log`
  does not exist (an empty database: no error); `strfmon` put a '.' for
  the empty `mon_decimal_point` of the POSIX locale (as glibc).
* In the headers: `<sys/shm.h>` lacked `SHM_FAILED` and `intptr_t`
  (POSIX.1-2024); `<dlfcn.h>` defines `Dl_info` always (Austin Group
  interpretation 1847, for TC1); `_XOPEN_UUCP` is the UUCP option's, not
  XSI's; and the namespace: `<signal.h>` showed `SS_ONSTACK`,
  `MINSIGSTKSZ`, `SIGSTKSZ`, `TRAP_*` without XSI and `POLL_*` (gone with
  STREAMS) without the extensions, `<sys/socket.h>` `PF_*` and
  `MSG_DONTWAIT` without XSI, `<sys/resource.h>` `struct rusage`,
  `PRIO_*`, `RUSAGE_*`, `RLIMIT_CPU` without XSI, `<sys/stat.h>` `mknod`
  without XSI, `<termios.h>` the output delays without XSI and `ECHOCTL`,
  `ECHOPRT`, `ECHOKE` without the extensions.
* `make install` copied over the installed `libc.so`, which running
  programs map: in a running Shall they (and the copy) crashed.  Each
  file is now copied beside its place and renamed into it.
* In c17 (`utilities/c17`): `INFINITY`, `NAN` and `HUGE_VAL` were `1.0/0.0`
  and `0.0/0.0`, evaluated when the program runs, so using them raised
  exceptions: c17 has `__builtin_inf`, `__builtin_huge_val`,
  `__builtin_nan` (and their `f`, `l`) now, which `<math.h>` uses;
  `<stdatomic.h>` included `<stdint.h>` and `<stddef.h>` (their macros
  polluted it) and had only macros for the functions that are not generic
  (now functions too, in `libc17rt.a`); `-E -dM`, the macros defined at
  the end, which the namespace suite needs.
* In the kernel (`kernel/README.md`): a pseudoterminal's slave opened
  with `O_NOCTTY` still became the controlling terminal; a process group
  whose members are zombies was taken not to exist (`tcsetpgrp`,
  `setpgid`); `open` of a directory with `O_CREAT` (without `O_DIRECTORY`)
  did not fail with `EISDIR` (POSIX.1-2024); `waitpid` for a process group
  did not wake when the last child left it; `poll` on a hung-up terminal
  gave `POLLOUT` with `POLLHUP` (they exclude each other) and no `POLLERR`.

A second round (6 November) took the easy ones of what was left: `pow(0,
-inf)` now has the pole error (`ERANGE`) that POSIX allows, `fma` the
domain error (`EDOM`) of an infinity times zero, also with a NaN `z`
("may"); `pthread_attr_getstack` before a stack is set gives no address
and the size (unspecified; was `EINVAL`); in the kernel, `F_GETLK` and
`F_OFD_GETLK` of `F_UNLCK` answer `F_UNLCK` (was `EINVAL`), and a new
pseudoterminal has the size 0x0 until its controller sets one, as on Linux
and the BSDs (the console stays 24x80).  The table is after it; the
overview of all three external suites is `share/doc/conformance.md`.  In
0.9.1 a UDP socket's address that connect() chose is chosen again by the
next connect() (it kept 127.0.0.1 after a reconnect from the loopback):
`connect-loopback-reconnect-wan-getsockname` now gives one of the outcomes
os-test lists as not yet judged, so it moves from the udp failures to
the 69 without an expectation.  Also in 0.9.1, `fnmatch()` follows the
2024 text where os-test showed it did not (glibc, musl and the BSDs do
neither): a backslash escapes in the name of a class, collating symbol or
equivalence class too (2.14.1: "regardless of whether or not the
<backslash> is inside a bracket expression"), and with `FNM_PATHNAME` a
bracket expression with a `/` in it stays one and matches no slash
(2.14.3's rule that makes its `[` ordinary is for filename expansion: the
shell and `glob()` split at slashes first, `pax` escapes such a `[` in its
pattern operands).  The fnmatch suite, run again with this library on the
host (not in QEMU), passes 61 of 67: 6 more, the table counts them.

What is left (200):

* 85 do not compile: 83 call `usleep` (78 of them in udp) or
  `gettimeofday`, which POSIX removed (2008, 2024) and Shall does not
  have; 2 (limits) use `_POSIX_TEXTDOMAIN_MAX`, which POSIX does not
  define (its minimum is `{_POSIX_NAME_MAX} - 3`): defects of the tests.
  Compiled with a shim (`shim.h`) for a diagnostic run, 39 of the 83 pass,
  32 have no known expectation, 12 fail (glibc: 38, 33, 12): the UDP
  questions below and `pututxline` without a utmp file (since 0.9.1 the
  first `pututxline` creates the database, and the image has `/run`).
* 69 have no expectation yet (udp's open questions, `sigaction` flags
  after `exec`).
* basic, 35 (and `pututxline`, which does not compile): 11 are QEMU's software x87 (`sinl`, `cosl`, `tanl`, a few
  complex functions; `remquo`'s quotient bits from `fprem1`), right on a
  real processor (checked in a chroot); 18 are the last ulp of long double
  results, which glibc misses too (correct rounding is not required); 3
  (`fma`, `fmaf`, `fmal`) are a defect of the test: its case 24 expects
  `fma(0, inf, 10.1)` to leave `errno` alone, where POSIX says a domain
  error shall occur (`EDOM`; glibc never sets it); `strfmon` aligns a
  positive amount with the negative one, as glibc (os-test reads the
  alignment rule otherwise); `sockatmark` needs TCP urgent data, which
  the kernel does not have.
* fnmatch, 6, and regex, 3: all os-test's "nonstandard" tests (invalid
  UTF-8, `\{,n\}` in a BRE).
* udp, 2: binding to the LAN's broadcast address is refused (Linux allows
  it); connecting again to 0.0.0.0 connects to the loopback, as on some
  systems, where os-test wants it to fail or to disconnect.

## Stage 17: the Open POSIX Test Suite on Shall's kernel

The suite (LTP git 43bf9cf, 4 November 2026: 1873 conformance tests, 253
of them compile-only, and 15 functional ones) was built inside the system
by c17, linked dynamically with libc.so, and run on Shall's kernel in QEMU
(300 s per test, as LTP); for comparison also in a chroot on Linux and
with glibc.  The runner is `tools/pts.sh` (POSIX sh; LTP itself is GPL and
stays outside the tree): `sh tools/pts.sh SRC OUT c17 300 [K/M]` with SRC
LTP's `testcases/open_posix_testsuite`; a run cut short continues where it
stopped, and K/M splits the list between machines.

* 111 do not compile, all for interfaces POSIX.1-2024 removed or that are
  not POSIX (see stage 14; `gettimeofday` alone is 22).
* Of the other 1777: 1711 passed on Shall's kernel at first, 1741 after
  the fixes below; glibc 1693 on Linux.  The run found, in this library:
  * read-write locks did not give way to waiting writers under
    `SCHED_FIFO`/`SCHED_RR` (Thread Execution Scheduling): a real-time
    reader now does not take the lock while a real-time writer of higher
    or equal priority is blocked on it (`pthread_rwlock.c`; readers are
    still preferred otherwise, as glibc's default).  Tested in `s_prio`.
  * `sched_getparam(pid, NULL)` crashed (the structure was written
    before the kernel was asked): now `EINVAL`, as Linux.
* and in the kernel (see `kernel/README.md`): timer overruns, a 1 ns
  timer interval that froze the system, `sigqueue` with signal 0 and its
  errors, sleeps continued after a stop, absolute `CLOCK_REALTIME` timers
  and sleeps when the clock is set (and relative ones not), `SA_NOCLDWAIT` with `wait`, write-back
  of an unlinked file's shared mapping, `msync(MS_INVALIDATE)` on locked
  memory, futex wake-ups by priority (`sem_post` order), `SIGSEGV` for
  `PROT_NONE`, `RLIMIT_AS`, and a limit on the number of mappings (whose
  absence made `mmap/24-1` map for ever).
* `shm_open/23-1` (a million `shm_open` calls from 1000 processes) passes
  on Linux but needs more than its 300 s under QEMU's emulation.
* `sched_setparam/23-2`, `25-2`, `25-3` and `sched_setscheduler/1-1` are
  defects of the tests: they switch to `SCHED_SPORADIC` without valid
  sporadic-server parameters, expect the sporadic parameters to apply to a
  process that is not `SCHED_SPORADIC`, or keep an old `errno`.
* Left, not bugs of Shall: tests of undefined behaviour (`pthread_join`
  and others on a joined thread) and of implementation-defined
  `aio_cancel` (a request in progress is cancelled here, as musl does).
* The 26 that do not pass with glibc either were each looked at; none is
  a fault of Shall.  7 need process contention scope (`PTHREAD_SCOPE_PROCESS`
  gives `ENOTSUP`, as POSIX allows: one scope is enough); 3 need a limit
  that is indeterminate here (`AIO_MAX` twice, `SEM_NSEMS_MAX`: no limit,
  `sysconf` -1) and 1 a 32-bit `off_t` (`mmap/31-1`); 10 test errors POSIX
  says "may" happen or behaviour it leaves undefined (`aio_error`/`aio_return`
  on a bad or retrieved aiocb, `EBUSY` from destroying a barrier or
  condition variable in use, an invalid mutex attribute object, `EPERM` for
  reading another process's scheduling or CPU clock, which Shall allows to
  all, as Linux); 5 are defects of the tests (`sched_setscheduler/19-2`
  forgets `<unistd.h>`, so `_POSIX_SPORADIC_SERVER` is not seen: with it
  the test passes on Shall's kernel; `aio_suspend/5-1` wants
  `_SC_ASYNCHRONOUS_IO` to be exactly 200112L; `pthread_rwlock_unlock/4-1`,
  `4-2` and `pthread_mutex_init/speculative/5-2` skip themselves on Linux,
  and Shall's kernel calls itself Linux).
* `clock_settime/8-1`, the 27th, was a bug of Shall's kernel (a relative
  sleep on `CLOCK_REALTIME` was lengthened when the clock was set back),
  now fixed; it fails with glibc here only because of the sandbox's clock
  (the host's Linux passes the same check in `kernel/tests/k_timer`).

## Stage 16: option groups, and where Linux departs from POSIX

* `_XOPEN_CRYPT`, `_XOPEN_REALTIME` and `_XOPEN_REALTIME_THREADS` are 1
  (in `<unistd.h>` and from sysconf): crypt/encrypt/setkey were done, every
  option the Realtime group requires is (prioritized I/O is optional for
  it), and the mutex protocols completed the Realtime Threads group.
* `nice()` and `setpriority(PRIO_PROCESS, 0 or getpid(), ...)` set the
  nice value of the process, as POSIX has it: Linux keeps one per thread,
  so the library sets it in every thread, under the thread-list lock
  (`__setnice`, beside `__setxid`).  For another process the kernel still
  changes only its first thread.  Test in `s_thread`.
* `pwrite()` writes at its offset also when the descriptor has `O_APPEND`
  (POSIX), by `pwritev2()` with `RWF_NOAPPEND` (Linux 6.9 and later;
  before that the kernel appends).  `aio_write()` still appends with
  `O_APPEND`, as POSIX requires of it.
* `O_EXEC` and `O_SEARCH` are Linux's `O_PATH`, which opens without
  looking at permissions; `open()` now checks execute (search) permission
  by the effective IDs on the file it opened (`faccessat2`, Linux 5.8) and
  fails with `EACCES`.  Tests in `s_files`.

## Stage 15: multi-character collating elements

A locale's `collating-element` entries (`<ch> from "<c><h>"`) are read
from `LC_COLLATE` (glibc's element 15: each element's bytes, wide
characters and collation sequence), and bracket expressions of regcomp()
and fnmatch() match them:

* `[[.ch.]]` matches "ch"; a name that is not a collating element of the
  locale is `REG_ECOLLATE` (fnmatch: the `[` is an ordinary character).
* `[[=x=]]` also matches the elements of x's equivalence class (the same
  first-level weights), and `[[=ch=]]` is valid when "ch" is an element.
* Range expressions and non-matching lists match single characters only;
  `[.ch.]` may be a range end point (its collation sequence).
* An element is an alternative to the characters of the set, so the
  longest match may take either: `[[.ch.]c]h` matches both "ch" and "chh";
  fnmatch tries both too.

Where POSIX leaves this open (ranges, non-matching lists), glibc's regex
matches elements as well, by rules of its own: in UTF-8 locales `[^a]`
matches "ch" but `[^c]` does not, in single-byte locales neither does.
glibc's own locales define some 870 elements (decomposed letters such as
"l·"), so with that `[^a]` and `[a-z]` would match two-character
sequences in grep.  Where POSIX is definite, glibc departs: `[[=ch=]]`
never matches, `[[.ch.]]` with `REG_ICASE` is `REG_ECOLLATE`, and its
fnmatch does not backtrack (`[[.ch.]c]h` does not match "ch") and lets
`[[.ch.]-d]` match "a".  `tests/s_collsym.c` (self-checking) covers
both functions in xx_XX (UTF-8 and ISO-8859-1), whose `collate.def` has
the contraction; the differential tests still agree with glibc.
Single-byte locales with collation rules now take range bounds from the
wide collation sequence, as glibc (elements have no other).

## Stage 14: the Open POSIX Test Suite

The conformance tests of the Open POSIX Test Suite (LTP's
`testcases/open_posix_testsuite`, 1873 tests) were built against this
library (static, gcc) and run; every test that did not pass was run
against glibc as well.

* 118 do not build (glibc: 28).  The 96 more are all for interfaces that
  POSIX.1-2024 removed (`gettimeofday`, `sighold`/`sigset`/`sigpause`/`sigignore`/`sigrelse`,
  `siginterrupt`, `setitimer`, `ctime_r`/`asctime_r`, `setpgrp`,
  `<ucontext.h>`) or that are not POSIX (`cpu_set_t`, `RLIMIT_MEMLOCK`,
  `<sys/param.h>`, `<features.h>`, `PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP`).
* Of the 1755 that build, 1650 passed at first.  17 failed here and
  passed with glibc; fixed since:
  * `aio_fsync` rejected `aio_reqprio` (which does not apply to it);
  * an invalid descriptor is now reported through `aio_error()` (`EBADF`),
    as glibc does, rather than by `aio_read`/`aio_write`/`lio_listio` at
    once: one bad request in a `LIO_NOWAIT` list no longer fails the call;
  * a woken waiter touched the condition variable again, so destroying
    one right after `pthread_cond_broadcast()` (which POSIX allows) could
    hang; process-shared ones are now waited for by `pthread_cond_destroy()`;
  * `pthread_rwlock_wrlock`/`rdlock` by the thread holding the write lock:
    `EDEADLK` (was a deadlock);
  * `pthread_attr_setschedparam`: `EINVAL` for a priority outside the
    policy's range;
  * `sem_unlink`, `shm_unlink`: `EACCES`, not `EPERM` (the sticky `/dev/shm`).
* The other 8 are not bugs: `aio_cancel` cancelling a request in progress
  (implementation-defined; musl too), `_SC_PRIORITIZED_IO` unsupported
  (the tests report UNTESTED; supported since, see the end), and `pthread_cancel`/`pthread_detach`/`pthread_join`
  on a thread already joined (undefined behaviour since POSIX.1-2008 TC1;
  glibc happens to keep the memory).

## Stage 13: the shared library and the dynamic linker

`make` (gcc) now also builds `libc.so`, `Scrt1.o` (PIE start file) and
`rcrt1.o` (static PIE).  `libc.so` is the C library and its own program
interpreter, as in musl; `tools/libc-cc` links with it when
`LIBC_LINK=dynamic` (gcc: a PIE; c17: a program at a fixed address, with
canonical PLT entries and copy relocations), makes static PIEs with
`LIBC_LINK=static-pie`, and shared objects with `-shared`.

| Interface | State | Notes |
|---|---|---|
| start-up | done | `_dlstart` → self-relocation with nothing that needs a relocation (`__dls1`), its own symbolic relocations (`__dls2`), a provisional thread pointer, the program (`AT_PHDR`), `LD_PRELOAD`, `DT_NEEDED` breadth-first, static TLS for all of them, relocation (dependencies first; libc.so again against the global scope), RELRO, the real thread pointer, the entry point; constructors in dependency order from `__libc_start_main` (`__libc.dl_init`), destructors in reverse at exit |
| relocations | done | RELATIVE (and DT_RELR), 64, PC32, GLOB_DAT, JUMP_SLOT, COPY, DTPMOD64, DTPOFF64, TPOFF64, IRELATIVE, IFUNC symbols; always immediate binding; GNU and SysV hash tables; hidden symbol versions skipped |
| search | done | `LD_LIBRARY_PATH` (not when AT_SECURE), `DT_RUNPATH` / `DT_RPATH` with `$ORIGIN`, libc.so's directory, /usr/local/lib, /lib, /usr/lib; libc.so, libm.so, libpthread.so, librt.so, libdl.so, libxnet.so, libcrypt.so are libc.so itself |
| `dlopen` | done | dependencies, `RTLD_LOCAL`/`RTLD_GLOBAL`, `RTLD_NOLOAD`, constructors; on an error (missing object, undefined symbol, initial-exec TLS) everything the call loaded is unmapped again |
| `dlsym` | done | handles (the object and its dependencies breadth-first), the program (the global scope), `RTLD_DEFAULT`, `RTLD_NEXT`, thread-local variables (the calling thread's address); the program's own symbols outside its dynamic symbol table too (from its ELF symbol table, as in a static link) |
| `dladdr`, `dlerror`, `dlclose` | done | `dlerror` per thread with the names involved; `dlclose` counts but does not unload |
| TLS | done | modules of the static TLS area in one layout (`__libc.tls_head`), a dtv per thread; dlopen'ed modules get a block per thread on first use (`__tls_get_addr`), freed at thread exit |
| static PIE | done | `rcrt1.o` relocates the program (assembly) before `__libc_start_main` |
| run as a command | done | `libc.so program args` (also programs that are not position-independent) |
| debuggers | done | `_r_debug` (the program's `DT_DEBUG`), `_dl_debug_state` |

The test suite runs in five ways: static (gcc, c17, and both crossed),
dynamic (gcc and c17 programs on the gcc-built libc.so) and static PIE
(gcc).  New: `s_dlopen.c` with the objects of `tests/dl/` (built by
`run.sh`): RTLD_LOCAL and RTLD_GLOBAL, a dependency found through
`$ORIGIN`, constructors once, destructors at exit, the same malloc,
stdio, errno and environ in program and object, dynamic TLS in four
threads, RTLD_NEXT, dladdr, and the three errors.

The POSIX utility collection, every utility linked dynamically
(`LIBC_LINK=dynamic CC=".../libc-cc gcc" sh run_tests.sh`): 6857 passed,
the same 10 failures of the environment as in stage 10.

Found on the way: glob() of a pattern ending in "/" (`"d1/"`) used an
unterminated buffer (in the static library the stack happened to hold
a zero); `weak.s` reached an undefined weak symbol PC-relatively, which
a PIE cannot (now through the GOT).
Later (the Shall bootstrap, building with the collection's own ar):
src/fenv/fenv.c and src/arch/x86_64/fenv.s both made a member fenv.o,
and POSIX ar -r keeps only the last of two members of one name; the
assembly file is now fenv_prim.s, and tools/genmk.sh refuses two
sources of one name.  `make install` (PREFIX, DESTDIR) is new.

Limits: `libc.so` exports its internal (reserved) names too; malloc
cannot be replaced by the program (calls inside libc.so bind to libc.so's
own); no lazy binding; no symbol versioning (an object built against
glibc does not load: it needs libc.so.6); text relocations are refused.

Later: c17 builds `libc.so` too (`make CCNAME=c17`).  c17 gained `-G`:
position-independent code (data and function addresses through the GOT,
calls through the PLT, except symbols of hidden visibility; general-
dynamic TLS, or initial-exec with `tls_model`), and shared libraries
from its link editor: functions bind to the library itself (as ld's
`-Bsymbolic-functions`), variables and function addresses stay
preemptible (a program's copy relocations and canonical PLT entries),
c17's run-time support is linked in hidden, an `ENTRY(_dlstart)` line in
`tools/libc.so.lds` gives the entry point.  All objects are
position-independent with either compiler, for libc.a and libc.so
alike.  The suite now runs in nine ways: static (gcc, c17, both crossed),
dynamic (gcc and c17 programs, each on the libc.so of either compiler),
static PIE; `tests/dl/` is built by the compiler under test.
`rcrt1.s` uses named local labels (c17's assembler takes one-digit
numeric ones only); `dlsym` needs `__builtin_return_address`, which c17
now has.

## Stage 12: multibyte locale codesets

Locales in EUC-JP, SHIFT_JIS, EUC-KR, GB2312 (EUC-CN), GBK, GB18030 and
BIG5 work: `mbrtowc` and its users, `wcrtomb`, `btowc`/`wctob`, the
classes and case maps, `wcwidth`, printf/scanf, the wide streams, regex,
`fnmatch`, `iconv` from `""`.  The converters are those of iconv
(`src/iconv/codec.c`, split out of `iconv.c`); a locale of such a codeset
has `cs == CS_MB` and the codec in `mbkind`.

| Interface | State | Notes |
|---|---|---|
| `mbrtowc` and the others | done | a partial character is kept in the `mbstate_t`; identical to glibc for every byte, every pair of bytes (whole and byte by byte), GB18030's four bytes and every code point (`tests/t_mbcs.c`), incl. glibc's ways: bytes it waits on, `btowc` of ASCII in Shift_JIS, GBK's 0x80, tag characters U+E0000-E007F written as nothing |
| regex | done | pattern and subject converted to UTF-8 with a map of the offsets (so trail bytes such as Shift_JIS 0x5c are no `\\`); bytes that are no character match only themselves |
| bracket ranges | done | in a locale with collation rules, by the collation sequence (`[a-z]` as glibc), regex and `fnmatch`, also in UTF-8 locales; enumerated block by block of the locale's table (`[a-zA-Z0-9_]` in nl_NL.UTF-8: 1 ms per regcomp) |
| `swprintf` | done | the format and the output through UTF-8: every wide character passes, as in glibc (before: only those of the locale's codeset); the locale's wide decimal point and separator |
| wide scanf | done | an engine of its own: characters, not bytes, for widths, `%c`, `%[` (wide scan sets) and `%n` |
| `strftime` | done | eras (`%EC`, `%Ey`, `%EY`, `%Ec`, `%Ex`, `%EX`) and alternative digits (`%O`), as glibc |
| `strptime` | done | eras and alternative digits too (src/time/era.c, shared with `strftime`): `%EY` tries each era's own format with that era fixed (so `平成元年`), `%EC` the longest era name, `%Ey` up to 4 digits counted in the era's direction and checked against its length, `%O` the longest alternative digits or else ASCII digits; identical to glibc for 588 round trips in ja_JP (3 codesets) and th_TH. Without eras or alternative digits they are the plain conversions, as POSIX says (glibc fails there: `%Ey` of "68", `%Od %Om`) |
| `strfmon` | done | the placement of symbol, sign and separators as glibc (stdlib/strfmon_l.c), incl. the alignment with `#` |
| `strcoll`, `wcscoll` | done | glibc's algorithm (see stage 11) |

The test locales (`tests/run.sh`) now include ja_JP.EUC-JP,
ja_JP.SHIFT_JIS, ko_KR.EUC-KR, zh_CN.GB18030, zh_CN.GBK, zh_CN.GB2312 and
zh_TW.BIG5; `t_mbcs.c` and `t_wide.c` are new; `t_locale`, `t_collate`
run in them too.  Character classes in regex (`[[:alpha:]]`) are also
enumerated block by block now (13 ms became 1 ms in a UTF-8 locale).

Deliberate differences: the wide streams fail with EILSEQ on a character
the codeset lacks (glibc writes `?`), as POSIX asks; `%C` keeps two
digits for years before 1000 (glibc: one, `"3"`).

## Stage 11: collation

| Interface | State | Notes |
|---|---|---|
| `strcoll`, `strxfrm`, `wcscoll`, `wcsxfrm` and the `_l` forms | done | by the locale's `LC_COLLATE` file (glibc's format, as glibc's and the POSIX utilities' localedef write it): contractions (`ch`), expansions (`ß` as `ss`), ignored characters, any number of levels, `backward`, `position`; results identical to glibc's in every case of `tests/t_collate.c` (strxfrm bytes and wide units, strcoll signs, orders); C, POSIX and C.UTF-8: code point order |
| regex, `fnmatch` | done | equivalence classes `[=x=]` by the first-level weights, as glibc; ranges by the collation sequence in a locale with rules (as glibc, stage 12); multi-character collating elements (`[.ch.]`, stage 15) |

`src/locale/collate.c`: the string is cut into collating elements by the
multibyte (or, for wide strings, the 3-level) table; each element's
weights give, level by level, a string of units: in order, except that
runs of elements whose rule is not `forward` at that level are taken
backward, and with `position` each element is preceded by the number of
ignored ones before it.  `strxfrm` writes the levels separated by 1;
`strcoll` follows glibc's algorithm (string/strcoll_l.c, stage 12), which
can order two strings other than their transforms do (`position` is
that of the first element's rule); results and their values are glibc's.
Sorting 100 000 random words in nl_NL.UTF-8 with qsort and strcoll takes
0.37 s (glibc: 0.24 s).

Also: in a locale with a charmap of its own, the classes of bytes
0x80-0xff come from its tables (`isalpha` etc.); `localeconv` gives
CHAR_MAX for the values a locale file leaves unspecified (-1), and
`strfmon` treats them so, as glibc does.

## Stage 10: the milestone — the POSIX utilities and c17 on this library

The utility collection (133 utilities, about 6 860 tests) builds and
passes its tests against this library, with either compiler:

    CC="/path/to/libc/tools/libc-cc gcc" sh run_tests.sh
    PATH=<c17 dir>:$PATH C17_LIBDIR=<c17 dir> CC="/path/to/libc/tools/libc-cc c17" sh run_tests.sh

and c17 is self-hosting on it: c17 built by gcc on this library
compiles c17 again (on this library); that c17 passes its own 286
tests, builds this library, and the library's tests pass in all four
combinations; with it the whole collection builds and passes again.

What the utilities found, and what changed here:

| Area | Change |
|---|---|
| `strerror`, `strsignal` | glibc's messages ("File name too long", "Unknown signal 65"), which scripts and test suites compare |
| stdio | an earlier write error no longer stops `printf` (the error indicator stays set, output goes on, as in glibc and musl); `fopen`/`fdopen`/`freopen` with "a" start at the end of the file (`ftell` gives the size) |
| `tcgetattr` | fills the whole `struct termios` (the kernel writes 19 of the 32 control characters), so structures compare equal |
| `getcwd` | deeper than the kernel reports (beyond 4096 bytes): found by walking up through ".." |
| regex | glibc's extensions `\<` `\>` `\b` `\B` `` \` `` `\'` `\w` `\W` `\s` `\S` (sed scripts use them) |
| locales | a locale made from a charmap of its own (localedef -f): its bytes are taken as in the POSIX locale, instead of the locale being refused |
| `<utmpx.h>` | glibc's `__UT_LINESIZE`, `__UT_NAMESIZE`, `__UT_HOSTSIZE` |

Failures that remain are those of the environment, which glibc has too
(bc rebuilt with the system's yacc/lex: none installed; diff -e checked
with the system's ed: none installed), a test that compares
`_POSIX_VERSION` with the system's getconf (glibc's 200809 against this
library's 202405), and a race in talk when both parties call at once
under the same name (seen with glibc as well).

## Stage 9: the rest of POSIX.1-2024

With this stage every header of POSIX.1-2024 exists (`<float.h>`,
`<stdarg.h>`, `<stddef.h>` and `<stdatomic.h>` come from the compiler).

| Header | State | Notes |
|---|---|---|
| `<search.h>` | done | `hsearch`: one table, linear probing, it grows (so `ENTER` fails only without memory; glibc's table is full at the prime above `nel`); `tsearch` family: an AVL tree; `posix_tnode` (new in 2024); `lsearch`/`lfind`, `insque`/`remque` (linear and circular lists) |
| `<cpio.h>`, `<tar.h>` | done | constants |
| `<fmtmsg.h>` | done | `MSGVERB`; messages, label check and return values as glibc's |
| `<ndbm.h>` | done | sdbm's dynamic hashing (file.dir: the split bits, file.pag: pages of 4096 bytes); a key and its value together up to 4080 bytes |
| `<aio.h>` | done | a thread per request; a write waits for earlier writes on the same descriptor and `aio_fsync` for everything before it; `aio_cancel` cancels blocked requests (pipes, sockets); notification by signal (`SI_ASYNCIO`) or thread; `lio_listio` in both modes |
| `<devctl.h>` | done | `posix_devctl` on `ioctl`: the command's encoded size must fit in `nbyte` |
| `<dlfcn.h>` | done | (stage 13: libc.so has the real one) in a static program `dlopen` of a file fails (nothing can be loaded into a static program); `dlopen(NULL)` gives the program, whose symbol table (from `/proc/self/exe`, unless stripped) `dlsym` and `dladdr` search; `dlerror` per thread; `Dl_info_t` (new in 2024) |
| `crypt`, `encrypt`, `setkey` | done | `crypt`: traditional DES, BSDi extended DES (`_`), MD5 (`$1$`), SHA-256 (`$5$`), SHA-512 (`$6$`), identical to libxcrypt's results except that `rounds=` below 1000 is raised to 1000 (the SHA scheme's specification; glibc did so) where libxcrypt fails. Not: bcrypt (`$2b$`), yescrypt (`$y$`, the default of current Debian and Fedora shadow files) |

The hash constants (MD5, SHA-256, SHA-512) are computed by bc
(`tools/mkhash.sh`); DES runs on tables made once from the standard ones.

## Stage 8: networking, interprocess communication, logging

| Header | State | Notes |
|---|---|---|
| `<sys/socket.h>` | done | all functions incl. `accept4`; `SOCK_CLOFORK` and `MSG_CMSG_CLOFORK` (new in POSIX.1-2024) native or emulated like `O_CLOFORK`; `struct msghdr`/`cmsghdr` with the POSIX member types (`int msg_iovlen`, `socklen_t` lengths): their padding is cleared before the kernel reads 64-bit fields |
| `<sys/un.h>`, `<netinet/in.h>`, `<netinet/tcp.h>`, `<arpa/inet.h>` | done | `inet_pton` strict as glibc's, `inet_addr` with the old short forms, `inet_ntop` as RFC 5952 |
| `<netdb.h>` | done | `getaddrinfo`/`getnameinfo` (numeric, `/etc/hosts`, then DNS), the hosts, networks, protocols and services databases; results, orders and error codes as glibc's in every case of `tests/t_inet.c` |
| `<net/if.h>` | done | names and indexes by ioctl, the list from `/proc/net/dev` |
| `<sys/ipc.h>`, `<sys/msg.h>`, `<sys/sem.h>`, `<sys/shm.h>` | done | the kernel's 64-bit structures; `sem_nsems` is `unsigned short` (POSIX; the kernel's field is wider) |
| `<mqueue.h>` | done | `mq_notify` with `SIGEV_THREAD` (a thread reads the kernel's notification from a netlink socket) |
| `<syslog.h>` | done | RFC 3164 lines on `/dev/log`, `LOG_CONS` to `/dev/console`; `%m` |
| `<sys/ioctl.h>` | done | not POSIX: `ioctl()` and the common requests |

**Resolver.** A stub resolver of its own (src/network/dns.c): A, AAAA
and PTR queries to the servers of `/etc/resolv.conf` (all at once, over
UDP; retried; over TCP when truncated), the `search` list and `ndots`,
CNAME chains for the canonical name. Results are sorted by RFC 6724 with
glibc's policy table, probing the source addresses with `connect()`;
`AI_ADDRCONFIG` restricts the families as glibc does. Tested against a
DNS server of the test's own on 127.0.0.1:53 (tests/s_net.c).

## Stage 7: threads

| Header | State | Notes |
|---|---|---|
| `<pthread.h>` | done | all of POSIX.1-2024: robust and process-shared mutexes, also with priority inheritance or protection (see below), condition variables, rwlocks and barriers; the `clock` variants; `pthread_atfork` |
| `<semaphore.h>` | done | unnamed (also process-shared) and named (`/dev/shm/sem.NAME`; reopening returns the same address); `sem_clockwait` |
| `<sched.h>` | done | `sched_setscheduler` returns the former policy (POSIX; Linux and glibc return 0) |
| `<threads.h>` | done | C11 threads on the same implementation; `thrd_t` is `pthread_t` |

**Design.** Threads are `clone()`d with their stack, thread-specific
data and TLS in one mapping; a detached thread unmaps its own stack on
the way out (`__unmapself`), `pthread_join` waits for the kernel to clear
the thread's `detach_state` (`CLONE_CHILD_CLEARTID`). Cancellation is
race-free: cancellation points go through `__syscall_cp_asm`, and the
`SIGCANCEL` handler moves a thread that is between the test of its
cancel flag and the end of the system call to `__cp_cancel`; condition
waits act on cancellation only after reacquiring the mutex (and pass on
a signal they consumed). Mutexes keep the owner's tid in the lock word,
the kernel's robust-futex protocol, so robust mutexes only need the
per-thread robust list. Private condition variables queue a record per
waiter (a signal always wakes a thread that was waiting); process-shared
ones use a sequence number and wake all. set*id calls are applied to
every thread (`SIGSETXID`); `SIGEV_THREAD` timers have a thread each
(`SIGTIMER`, `SIGEV_THREAD_ID`). `fork()` takes the thread-list lock and
leaves the child with one thread. The public `pthread_` names of objects
the C11 functions use are weak aliases, so `<threads.h>` stays within
the ISO C namespace. Default stack 2 MiB (reserved lazily), guard one
page.

Mutex protocols (after stage 13).  `PTHREAD_PRIO_INHERIT` uses the
kernel's PI futexes, whose lock word is the one above: uncontended a
compare-and-swap, contended `FUTEX_LOCK_PI` (`FUTEX_LOCK_PI2` for
`CLOCK_MONOTONIC` timeouts), which queues the waiters by priority and
lends the owner the highest of theirs.  Robust PI mutexes are marked in
the robust list (bit 0 of the pointer), so the kernel hands them to a
waiter when the owner dies; since the kernel owns the word of a
contended PI mutex, "not recoverable" is a flag in the mutex.
`PTHREAD_PRIO_PROTECT` keeps the ordinary word; the thread counts the
ceilings it holds and, if it is `SCHED_FIFO` or `SCHED_RR`, runs at the
highest of them while above its own priority (a ceiling below its own
priority: `EINVAL`).  `SCHED_OTHER` threads have no priorities on Linux,
so they are not raised; their locks succeed (glibc's fail with
`EINVAL`).  `pthread_mutex_setprioceiling` takes the mutex without its
protocol; a waiter that gets the mutex after a change takes on the new
ceiling.  Tested in `tests/s_prio.c`, which reads the effective priority
from `/proc` (boosts by inheritance and by ceilings; skipped where
real-time threads are not allowed).  Found on the way: `trylock` of an
error-checking or robust mutex by its owner returned `EDEADLK`; POSIX
says `EBUSY` ("including the current thread").

Tested by tests/s_thread.c (self-checking): creation, detaching,
stacks, TLS, errno, all mutex kinds incl. robust ones whose owner thread
or process dies, condition variables (also between processes), rwlocks,
barriers, spin locks, once, TSD destructors (re-armed), cancellation
(at `read`, at condition waits with a cleanup handler, before blocking,
asynchronous, disabled, at `pthread_join` and `sem_wait`), signals to
threads, fork in a thread with atfork handlers, set*id across threads,
`SIGEV_THREAD` timers, semaphores, scheduling, C11 threads, and stdio,
malloc and per-thread locales under threads; run 75 times on two CPUs
and 10 times pinned to one.

## Stage 6: mathematics

| Header | State | Notes |
|---|---|---|
| `<math.h>` | done | all C17 functions in three precisions; XSI `j0`..`yn`, `signgam`; the `M_` constants of POSIX.1-2024 with their `l` forms (`M_EGAMMA`, `M_PHI`, `M_SQRT3`, ...); classification and comparison macros by GCC built-ins, or by `_Generic` over functions for c17 |
| `<fenv.h>` | done | x87 and SSE kept in step (flags of either count; `fesetround` sets both); `FE_DFL_ENV`; layout and values as glibc's |
| `<complex.h>` | done | all functions in three precisions; `CMPLX*` by `__builtin_complex` (c17's own `<complex.h>` includes this one); no imaginary types |
| `<tgmath.h>` | done | `_Generic`, for gcc and c17 alike |

**Method.** The functions compute in the x87 80-bit format and round
once: the x87 instructions (`fsin`, `fyl2x`, `f2xm1`, `fpatan`, `fprem`...)
through the small functions of `arch/x86_64/x87.s`, with careful argument
reduction (Cody-Waite below 2^23, Payne-Hanek with 16768 bits of 2/pi
above, so `sinl(1e4000L)` is right); `pow` with `log2 x` in double-long-
double precision; `erf`/`erfc` by series and continued fraction;
`lgamma`/`tgamma` by the zeta series around 1 and 2 and Stirling;
Bessel functions by Miller's recurrence with the Neumann series, Hankel's
expansion for x >= 25; `fma` exactly in integer arithmetic, in every
rounding mode. Tables are generated by bc (`tools/mktwoopi.sh`,
`tools/mkzeta.sh`; `tools/mkbessel.sh` for the test's exact values).
`math_errhandling` is `MATH_ERRNO | MATH_ERREXCEPT`; as glibc, underflow
sets `ERANGE` only when the result is zero.

**Accuracy** (tests/u_math.c, u_complex.c against glibc; s_math.c
against exact values). float and double results are correctly rounded in
almost all cases: where they differ from glibc by more than 1 ulp
(`cbrt`, `log10f`, `sinh`, `tanhf`, `acoshf`, `erfc`, `lgammaf`, `tgamma`,
`jn`, `yn`: glibc is off by 2 to 240 ulps), the cases checked with bc
showed this library right. The Bessel functions are within 0.5 ulp of
the exact values at all test points, except near their zeros (absolute
error < 2^-62). long double: 0 ulp from glibc in the tests for the
elementary functions; erf, lgamma, tgamma within a few ulps.

`libm.a`, `librt.a`, `libpthread.a`, `libxnet.a` exist (empty) so that
`-lm` and the like work; `tools/libc-cc` puts this library's directory
first in the search.

## Roadmap

All headers of POSIX.1-2024 exist, and the POSIX utilities and c17 run on this library (stage 10).

Done through stage 18 (shared library, dynamic linker, static PIE, libc.so built by c17 as well; the Open POSIX Test Suite; os-test; multi-character collating elements; option groups; the nice value, pwrite and O_EXEC/O_SEARCH made to conform on Linux).  Possible next steps: symbol visibility and versioning for libc.so; bcrypt/yescrypt.

## Open items

* libc.so exports its internal (reserved) names; malloc is not interposable (calls inside libc.so bind to it).
* `crypt`: bcrypt and yescrypt are missing.
* Priority ceilings: a thread that changes its own priority while holding `PTHREAD_PRIO_PROTECT` mutexes is not re-raised until it takes or releases one, and `pthread_getschedparam` reports the raised priority.
* A locale with a charmap of its own: bytes 0x80-0xff convert to wide characters as in the POSIX locale (the file does not say which characters they are), so wide case mappings of them do not apply.

Where Linux itself stands between this library and POSIX (what a kernel
of Shall's own would settle):

* `O_CLOFORK`/`FD_CLOFORK` are emulated on Linux (a table the library
  keeps): not atomic against a `fork()` in another thread between `open()`
  and the table entry, and invisible to descriptors passed by other means.
  Shall's kernel has them, and the library then leaves them to it.
* Credentials and the nice value are kept per thread by Linux; set*id and
  `nice()`/`setpriority()` of the process itself reach every thread through
  the library (signals for set*id, so not atomically), but `setpriority()`
  and the `sched_*` functions on another process change only one of its
  threads.
* `pwrite()` at its offset with `O_APPEND` needs Linux 6.9, the
  `O_EXEC`/`O_SEARCH` permission check Linux 5.8 (`faccessat2`).
* Access times: Linux's default mount option `relatime` updates them less
  often than POSIX asks (`strictatime` does as POSIX says).
* Options not offered on Linux: `_POSIX_SPORADIC_SERVER` and
  `_POSIX_THREAD_SPORADIC_SERVER` (no such scheduling policy in Linux),
  `_POSIX_TYPED_MEMORY_OBJECTS` (no such memory in Linux): the three are
  0 in `<unistd.h>` and `sysconf()` asks the kernel (Shall's has them).
  Not offered: the 32-bit programming environments (ILP32_OFF32 and
  ILP32_OFFBIG).  LPBIG_OFFBIG is offered: it is the LP64_OFF64
  environment, whose types meet its minimum widths.  The FORTRAN Runtime
  Utilities option (`_POSIX2_FORT_RUN`) is offered: it is `asa` alone.
  (`fort77` is not in POSIX.1-2024: it was removed in Issue 8.)
* `_POSIX_RAW_SOCKETS`: Linux has them for privileged processes; Shall's
  kernel for root, on its loopback interface.

## After stage 16 (with Shall's kernel)

* Prioritized I/O (`_POSIX_PRIORITIZED_IO`, `_POSIX_PRIO_IO`,
  `AIO_PRIO_DELTA_MAX` 20): the worker thread of an `aio_read`/`aio_write`
  of a thread with `SCHED_FIFO` or `SCHED_RR` runs with that policy at the
  thread's priority less `aio_reqprio`, so the scheduler serves requests
  in priority order; for other policies `aio_reqprio` has no effect (POSIX
  leaves that unspecified).  Tested in `s_aio` through a `SIGEV_THREAD`
  notification, which runs in the worker.
* `getaddrinfo` with `AI_ADDRCONFIG` on a system with only a loopback
  interface: `AF_UNSPEC` is not narrowed (it was `EAI_NONAME`), as glibc.
* Close-on-fork is the kernel's where it has it (see above).
* The sporadic server ([SS], [TSP]): `SCHED_SPORADIC` is 4 (a number Linux
  does not use, and refuses), `struct sched_param` has POSIX's
  `sched_ss_low_priority`, `sched_ss_repl_period`, `sched_ss_init_budget`
  and `sched_ss_max_repl` after `sched_priority`; the kernel reads and
  writes the long form for this policy only, so `sched_getparam` and
  `pthread_getschedparam` clear the structure first, and
  `pthread_setschedprio` passes the whole of it.  `pthread_attr_t` keeps
  its size: a server's other parameters are packed into bits the others
  leave free (budget and period in microseconds; `pthread_attr.c`), and
  `posix_spawnattr_t` keeps them in its padding.  `sysconf(_SC_SPORADIC_SERVER)`,
  `_SC_THREAD_SPORADIC_SERVER` and `_SC_SS_REPL_MAX` (32) ask whether the
  kernel takes the policy.  Tested in `tests/s_sporadic.c` (a 20 ms budget
  every 100 ms against a busy SCHED_FIFO thread between its priorities:
  about 200 ms of each second).
* Typed memory objects ([TYM]): `posix_typed_mem_open("/NAME", oflag,
  tflag)` opens `/dev/tym/NAME` (O_CLOEXEC and O_CLOFORK honoured) and
  gives the kernel tflag; `posix_typed_mem_get_info` and `posix_mem_offset`
  ask the kernel (the latter through `/dev/tymctl`).  Without them
  (Linux): ENOENT, ENODEV, EACCES.  `sysconf(_SC_TYPED_MEMORY_OBJECTS)`
  looks for `/dev/tymctl`.  Tested in `tests/s_typedmem.c`.
