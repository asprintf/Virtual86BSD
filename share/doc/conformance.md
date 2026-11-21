# V86BSD and external conformance test suites

V86BSD was checked with three test suites written by others:

* **os-test**, from Sortix: 5816 tests of POSIX headers, the namespace and C library functions;
* **the POSIX Shell Tests**, yash's test suite: 10,979 cases of the shell language and its built-in utilities;
* **the Open POSIX Test Suite**, from LTP: 1888 tests of the threads, real-time and signal interfaces.

Each suite is compared here with other implementations: glibc on Linux for os-test and the Open POSIX Test Suite, and yash, dash and bash for the shell tests. The results are for V86BSD 0.9.1, from 5–6 November 2026. os-test and the Open POSIX Test Suite were run again on 11 November 2026, with the tree as it was after the move to an OpenBSD-style layout and with KVM; their tables and text below are those of the latest run (the third, after `gettimeofday()`, `usleep()` and the other XSI interfaces of older editions were added: `share/doc/legacy.md`), and say where it differs from the earlier ones.

None of the suites is part of this tree. The scripts that run them are:

* `lib/libc/tools/ost/` for os-test (`vmrun.sh` does the whole run, with a progress bar);
* `regress/bin/sh/tests/posix_shell_tests.sh` for the POSIX Shell Tests;
* `lib/libc/tools/pts.sh` for the Open POSIX Test Suite (`lib/libc/tools/ptsrun.sh` does the whole run, with a progress bar).

A test that fails is not always a fault in the system it tests. For each suite, every result below that is not a pass is accounted for. Each is put in one of four classes:

* a limitation of V86BSD;
* a defect in the test;
* something the standard leaves open;
* a deliberate choice.

## Summary

| suite | V86BSD | comparison |
|---|---|---|
| os-test | **5666 / 5816** pass | glibc 2.39 on Linux: 5228 |
| POSIX Shell Tests | **10,940** pass, **8** fail (31 skipped) | yash 2.55: 0 fail; dash 0.5.12: 1,271; bash 5.2 as `sh`: 1,634 |
| Open POSIX Test Suite | **1812 / 1848** that compile pass | glibc on Linux: 1693 |

Every header test and every namespace test of os-test passes on V86BSD. glibc passes 3471 of the 3758 header tests and 130 of the 159 namespace tests.

## On the FreeBSD kernel (18 November 2026)

The numbers above are those of V86BSD's own kernel. V86BSD now runs on FreeBSD's kernel (releng/15.2, `sys/amd64/conf/V86`, with the patches of `distrib/freebsd/patches-sys/`) and this libc built for its ABI. os-test and the Open POSIX Test Suite were run again in QEMU with KVM: `sh lib/libc/tools/gate.sh ost|pts WORKDIR` builds the tests on the host with clang and runs them one by one in the VM (a time limit each); `ONLY=file` runs only the tests named in the file.

| suite | V86BSD on FreeBSD's kernel | on V86BSD's kernel |
|---|---|---|
| os-test | **5749 / 5863** | 5666 / 5816 |
| Open POSIX Test Suite | **1805 / 1876** that compile | 1812 / 1848 |
| `regress/lib/libc` | 47 / 47 (static, dynamic, `-smp 4`) | |

os-test (git 5d947a8, 19 November 2026; the column on V86BSD's own kernel is os-test 8e790a4), by suite: basic 1184/1187, fnmatch 67/67, include 3758/3758, io 55/55, limits 126/126, malloc 3/3, namespace 159/159, paths 48/48, process 24/24, pty 29/29, regex 96/96, signal 31/32, stdio 25/25, udp 98/207, unix 46/47. The unix suite (47 tests of `AF_UNIX` stream sockets: `bind`, `listen`, `connect`, `accept`, `poll`, `shutdown`, `sockatmark` in each state) is new in 5d947a8; the same update fixed the tests for `fma`, `pututxline`, `TEXTDOMAIN_MAX` and the invalid UTF-8 patterns of fnmatch and regex.

Kernel patches (`distrib/freebsd/patches-sys/`) that the gates asked for, each from a read of FreeBSD's source:

* **0002, io 32:** FreeBSD 15.2 has no `F_OFD_*` locks. They are `fcntl` commands 25 to 27 now: byte ranges owned by the open file description, with `flock(2)`'s ownership and its release at the last close (`kern_descrip.c`, `kern_lockf.c`).
* **0003, process 3 and pty 1:** `waitpid(-pgid)` sleeps on the parent and only a child's exit woke it, so a wait on a group that had just become empty never returned `ECHILD`; `setpgid` and `setsid` wake the parent now. A new process group was not marked orphaned (`enterpgrp` began with `pg_flags = 0`), so `tcsetpgrp` from it sent `SIGTTOU` instead of failing with `EIO`.
* **0004, signal 2:** a signal action kept `SA_ONSTACK` over `exec` (`execsigs`).
* **0005, pty 2:** a pseudoterminal whose controller was closed read `POLLIN|POLLHUP` without `POLLERR` and `write` failed with `ENXIO`; POSIX wants `POLLERR` and `EIO`.
* **0006, signal 3:** `ppoll` and `pselect` restored the signal mask before a pending signal could be delivered when a descriptor was ready already; they deliver it now.
* **0007, udp 7:** `sosend_generic` sent `SIGPIPE` for `EPIPE` on every kind of socket; POSIX generates it for `SOCK_STREAM` and `SOCK_SEQPACKET` only, so a datagram socket shut down for writing gets `EPIPE` alone now.
* **0008, unix 1:** `SIOCATMARK` on a listening socket failed with `EINVAL`; `sockatmark()` returns 0 for a socket with no mark.
* **0009, unix 2:** a stream or seqpacket socket that was never connected (new, or only bound) reported no event to `poll()`. `read()` and `write()` on it fail with `ENOTCONN`, which POSIX says shall be reported as `POLLERR` (`uipc_sopoll_stream_or_seqpacket`, and `sopoll_generic` for the other connection-oriented protocols). FreeBSD, Linux, OpenBSD and NetBSD report no event or `POLLOUT|POLLHUP` here; of the systems on os-test's site only Sortix reports `POLLERR`. The patch follows the text, not the other systems, and it also changes `poll()` of a TCP socket that is polled before `connect()`.
* **0010, udp 3:** a datagram socket that is connected takes datagrams from its peer only: POSIX says `connect()` "limits the remote sender for subsequent recv() functions". Datagrams of other senders that were queued already were still delivered (FreeBSD, Linux, the other BSDs and Solaris all do); `udp_connect` drops them now (`udp_drop_foreign`, IPv4 only).

The libc fixes the gates found: `connect(AF_UNSPEC)` on a datagram socket (the kernel dissolves the association and then refuses the family), `accept()` on a datagram socket (`EOPNOTSUPP`), `shm_open()` and `shm_unlink()` take names without a slash (the kernel wants one) and refuse components over `NAME_MAX`, `munmap()` refuses an unaligned address (the kernel rounds it down), `mmap()` of more than the address space fails with `ENOMEM` (the kernel says `EINVAL`), `raise()` does not queue a non-real-time signal that is pending already (`thr_kill` queues every instance), `clock_nanosleep()` and `posix_fadvise()` return the error number the kernel puts in the result (`kern_posix_error`), `MQ_PRIO_MAX` is 64 and `SIGQUEUE_MAX` 128 as the kernel has them, `_POSIX_SPORADIC_SERVER` is -1, and `statfs()`, `fstatfs()` and `getfsstat()` exist. `/etc/sysctl.conf` sets `net.inet.ip.connect_inaddr_wild=1`, which the 4 broadcast `connect` tests of the udp suite need.

What is not good in os-test: udp has 109 (108 have no expected outcome, 7 of them `send` or `sendto` after `shutdown(SHUT_WR)` on a datagram socket, which raised `SIGPIPE` before patch 0007; the 1 that is judged bad is `sendto-any-so-error`: `sendto()` to 0.0.0.0 gives `EACCES`, because FreeBSD checks the connect-time rule for 0.0.0.0 only in `connect()` and `ip_output` takes 0.0.0.0 for a broadcast address; POSIX does not say what sending to INADDR_ANY does, os-test lists four outcomes as unknown (success, `EDESTADDRREQ`, `ECONNREFUSED` in `SO_ERROR`, `ENETUNREACH`) and `EACCES`, which stock FreeBSD also gives, is not among them), basic 3, unix 1 and signal 1 (`sigaction-exec-flags`, no expected outcome). The 8 tests that os-test marks `[nonstandard]` pass. POSIX leaves both open: XCU 2.14 says matching a string with bytes that are not part of a valid character is unspecified, so `fnmatch()` lets `?` and a non-matching list reject an invalid byte in a UTF-8 locale (`*`, a bracket expression naming it and a literal identical byte still match it; 6 tests); and XBD 9.1 says that a construct outside the grammar, as `\{,3\}`, has undefined results, which may enable an extended syntax, so a BRE interval may omit the lower bound, as an ERE's did already (2 tests). The limits tests of `TEXTDOMAIN_MAX` and the `fma` tests, which were wrong in os-test 8e790a4, were corrected upstream and pass. In basic: `strfmon` and `strfmon_l` with a left precision (`#3`) expect no padding, but POSIX says the positive and negative formats are padded to an equal length, so a positive amount gets a space for the `-`. POSIX and C give no accuracy bound for the math functions, and IEEE 754-2019 clause 9.2 requires correct rounding of the operations of its table 9.1 only where an implementation defines them as its conforming operations, a recommendation that conformance (3.1.2) does not depend on (it has no gamma function or complex operations); os-test's expected values come from glibc's `_Float128` functions rounded to long double (`misc/genbasic.c`), so they are the correctly rounded ones, and a test passes only on an exact match. `logl`, `log2l`, `log10l`, `atanhl`, `sinl` and `cosl` are correctly rounded (`math/__ddl.c`: sums of two long doubles, about 2^-115, one final rounding; 4000 random arguments each agree with a 100-digit reference, where glibc's long double functions differ in up to half of them for `log10l`); `sinhl`, `coshl`, `asinhl`, `tgammal` (the Stirling series of ln Gamma after shifting the argument above 50, reflection for negative arguments; exact factorials) and, for finite arguments of moderate size, the complex `cexpl`, `csinhl`, `ccoshl`, `ctanhl`, `catanhl`, `casinhl`, `cacoshl` and `cpowl` are computed the same way (`math/__ddl.c`; so are `csin`, `ccos`, `ctan`, `catan`, `casin`, `cacos` and the double and float versions, which are these rounded again); thousands of random arguments per function agree bit for bit with an exact reference, signed zeros included, where glibc's long double `cexpl`, `csinhl` and `ctanhl` differ from it in half of them or more; outside that range, near zero, the poles and the branch points, they are as before; the other long double functions are within a few units in the last place; `tcsendbreak` on a pty gives `ENOTTY` (the pty driver, `sys/kern/tty_pts.c`, does not implement `TIOCSBRK`, and `tty.c` has no generic one).

The unix test that is not good, `server-sockatmark`, is a defect in os-test's expectation. The test prints `0` as `bind-sockatmark`, `sockatmark` and `bind-listen-sockatmark` do, and the expected outcome file says `exit: 0`, which the harness writes only when a test prints nothing; no system can produce it, and on os-test's site it fails on FreeBSD, Linux, OpenBSD and NetBSD alike.

Open POSIX tests that do not pass, for a reason in the kernel or in the test: no `SCHED_SPORADIC` (7), `sched_setaffinity` helpers that exist for Linux only (`affinity.h`, 2), `seteuid()` leaves the real user ID, which the scheduler checks first (`p_cansched`, 4), the priority of `SCHED_OTHER` is the scheduler's own (`ksched_getparam`, 3), `sem_wait()` is not restarted after a signal with `SA_RESTART` (the `_umtx_op` wait maps `ERESTART` to `EINTR`, 1), `SA_NOCLDSTOP` is kept for `SIGCHLD` only (1), negative clock IDs are CPU-time clocks (`kern_clock_getres`, 1), kernel timers exist for `CLOCK_REALTIME`, `CLOCK_MONOTONIC` and `CLOCK_TAI` only (`itimer_start`; `timer_create` of a CPU-time clock, 3), `sigwaitinfo` of `raise()` has `si_code` `SI_LWP` where the test wants `SI_USER` (1), `mmap/24-1` maps shared memory until FreeBSD runs out of kernel memory, as it has no limit on the number of mappings, and stops the VM (the gate skips it), and an unlock by a thread that is not the owner (`pthread_cond_timedwait/2-3`, undefined), and `mmap/11-4`: bytes written past the end of the file in its last page are never written to the file (`vnode_pager_generic_putpages` clamps the write to the file size) but stay in the cached page, which the test's second mapping still sees, where Linux zeroes the tail when it writes the page out. About 27 more (aio on sockets, `timer_getoverrun`, `mlockall`, the thread guard page, `sem_post` order, `clock_settime` timers) have no proven cause yet.

## How the suites were run

**On V86BSD:**

* The tests were built inside V86BSD by its own compiler, c17, against V86BSD's C library.
* os-test and the Open POSIX Test Suite then ran on V86BSD's own kernel in QEMU. The first run used the CPU emulated in software (no KVM); the second used KVM (about 3 minutes for os-test's build and run, 14 for the Open POSIX Test Suite).
* os-test's header and namespace suites only compile, so they were done in a chroot.
* The POSIX Shell Tests need yash for their runner, so they ran on the Linux host. The shell under test was V86BSD's `sh`, linked statically with V86BSD's C library.

**The comparisons** ran on the host:

* Linux 6.18 in a cloud sandbox with limited networking;
* gcc 13.3 with glibc 2.39;
* yash 2.55, dash 0.5.12 and bash 5.2.21 (bash invoked as `sh`, which makes it follow POSIX).

Some results are affected by the environment:

* The sandbox's restricted networking may have changed some of glibc's UDP results.
* Under QEMU's software CPU, the x87 floating-point instructions give slightly different results than real hardware. 11 os-test tests failed because of this in the first run. Under KVM 10 of them pass (see the basic suite below).

## os-test

The tests come from os-test as of 5 November 2026 (git 8e790a4, 19 October): 5816 tests in 14 suites. The judgements are those of os-test's own report generator, `misc/html`. A test os-test has no expected outcome for counts as not passed.

| suite | V86BSD | glibc / Linux |
|---|---|---|
| basic (a test of every function) | 1162 / 1187 | 1077 / 1187 |
| fnmatch | 61 / 67 | 49 / 67 |
| include (headers) | **3758 / 3758** | 3471 / 3758 |
| io | 55 / 55 | 53 / 55 |
| limits | 124 / 126 | 119 / 126 |
| malloc | 3 / 3 | 3 / 3 |
| namespace | **159 / 159** | 130 / 159 |
| paths | 48 / 48 | 48 / 48 |
| process | 24 / 24 | 21 / 24 |
| pty | **29 / 29** | 25 / 29 |
| regex | 93 / 96 | 83 / 96 |
| signal | 31 / 32 | 29 / 32 |
| stdio | 25 / 25 | 25 / 25 |
| udp | 94 / 207 | 95 / 207 |
| **total** | **5666 / 5816** | **5228 / 5816** |

The first run of 0.9.1 (under QEMU's software CPU) passed 5616; the second, under KVM, 5626; the third, with the XSI interfaces of older editions, 5666 (no test that passed before fails). V86BSD's very first run passed 5501 tests. The fixes made after it are listed in `libc/STATUS.md`, stage 18; with them 0.9 passed 5610. They include errors in the floating-point exceptions and `errno` settings of the math functions, header namespace leaks, and some pseudoterminal and process-group behaviour in the kernel. 0.9.1 makes `fnmatch()` follow the 2024 text in two places (see below). The fnmatch suite was run again with 0.9.1's library, on the host rather than in QEMU, and passes 6 more tests; the table counts them.

### The 150 that do not pass on V86BSD

**None fails to compile now.** In the first runs 85 did not: 83 call `usleep()` or `gettimeofday()` (78 of them in the udp suite), which POSIX removed in 2008 and 2024, and 2 use `_POSIX_TEXTDOMAIN_MAX`. V86BSD now declares the two functions for the macros of the editions that have them (`share/doc/legacy.md`), and os-test's basic and udp suites are compiled with `_GNU_SOURCE`, which declares everything. Of the 83, 39 passed, 32 have no expected outcome and 12 fail, as the diagnostic run with a replacement for both functions (`lib/libc/tools/ost/shim.h`, no longer needed) had predicted; glibc got 38, 33 and 12.

* The 2 `_POSIX_TEXTDOMAIN_MAX` tests (limits) are a defect in the test: POSIX does not define it; the minimum it gives is `{_POSIX_NAME_MAX} - 3`.

**101 have no expected outcome yet.** These are open questions in os-test itself, mostly about UDP (one is the UDP reconnect test described below); 32 of them are the tests that now compile.

**basic, 25** (36 under QEMU's software CPU):

* 11 were QEMU's software x87: `sinl`, `cosl` and `tanl` of large arguments, five complex functions, and the `remquo` family (QEMU emulates the `fprem1` instruction wrongly). Under KVM 10 pass. The eleventh, `cosl(90.01)`, is off by one unit in the last place on the real processor, and is counted with the next group.
* 19 are off by one unit in the last place of a long double result (15 complex functions, among them `cpow`, and `atanhl`, `cosl`, `log10l`, `tgammal`). POSIX does not require correct rounding, and glibc misses the same results (checked for the 18 of the first run). They fail the same way every time under KVM.
* 3 (`fma`, `fmaf`, `fmal`) are a defect in the test. Its case 24 expects `fma(0, inf, 10.1)` to leave `errno` alone. POSIX says a domain error *shall* occur, which means `errno` is `EDOM`. V86BSD does what POSIX says; glibc never sets `errno` in `fma`.
* 2 are `strfmon` and `strfmon_l`. V86BSD pads a positive amount to the width of the negative one, as glibc does; os-test reads POSIX's alignment rule differently.
* 1 is `sockatmark`, which needs TCP urgent data. V86BSD's kernel does not support it: this is a limitation of V86BSD.

**fnmatch 6, regex 3:** all 9 are tests os-test itself marks "nonstandard": rejecting invalid UTF-8, and `\{,n\}` in basic regular expressions.

Two things os-test found in `fnmatch()` were fixed in 0.9.1. Both follow the letter of POSIX.1-2024, and glibc, musl and the BSDs do neither:

* A backslash now escapes inside the name of a character class, collating symbol or equivalence class (`[[:al\num:]]` is `[[:alnum:]]`, `[[.\\.]]` is the backslash): XCU 2.14.1 has a backslash escape "regardless of whether or not the <backslash> is inside a bracket expression". 4 tests.
* With `FNM_PATHNAME`, a bracket expression that contains a `/` is still a bracket expression; it just never matches a slash. `fnmatch()` takes XCU 2.14.1 and 2.14.2, not 2.14.3's rule for filename expansion that makes such a `[` ordinary. The shell and `glob()` split a pattern at slashes before matching, so they keep that rule; `pax` applies it itself to its pattern operands. 2 tests.

**udp, 13:**

* Binding to the LAN's broadcast address is refused; Linux allows it. This is a limitation of V86BSD's kernel.
* Connecting again to 0.0.0.0 connects to the loopback, as on some systems. os-test wants this to fail or to disconnect the socket.
* 11 are tests that could not be built before. Compared with os-test's own run on Linux (this host: Linux 7.2, glibc 2.44; its udp suite gives 91 passes, 98 with no expected outcome and 18 failures), 7 of them fail there as well (`pair-shutdown-r-send-recv`, `pair-shutdown-rw-send-recv`, `bind-connect-self-shutdown-r-send-recv`, `sendto-loopback-0-so-error` and three `trio-send-wrong-y-connect-*`), and 4 (`bind-socket-sendto-first-shutdown-r-recv` and its three variants) have no expected outcome there. They were not analysed one by one.

A UDP socket that was connected on the loopback used to keep 127.0.0.1 as its address when it was connected again to an outside address. 0.9.1 fixes this; os-test has no settled expectation for that test, so it counts among the 101 with no expected outcome.

**signal 1, pty 0:** the signal test that is not good has no expected outcome.

## The POSIX Shell Tests (yash's test suite)

The suite is posix-shell-tests (git b1acf5f, GPL-2): 120 files and 10,979 cases. Most of the cases check what happens to every signal in every context: in subshells, in background lists, in traps, with and without job control. The runner is yash. Each file ran on a pseudoterminal of its own, because the job-control tests need a terminal and hang without one, for every shell.

| shell | passed | failed | skipped |
|---|---|---|---|
| **V86BSD `sh`** | **10,940** | **8** | 31 |
| yash 2.55 | 10,957 | 0 | 22 |
| dash 0.5.12 | 9,674 | 1,271 | 31 |
| bash 5.2.21 as `sh` | 9,166 | 1,634 | 29 |

A few test files cut bash off at the 300-second limit; that is why bash's total is lower. The suite was written for yash, so yash passing everything is expected.

V86BSD's first run had 970 failures, and 0.9 had 15. The fixes made after the first run are listed in `utilities/sh/README.md`. Most were about signals:

* signals ignored on entry, and SIGINT and SIGQUIT in background lists;
* job control giving the terminal back when the shell ends;
* traps in subshells.

The others were the order of redirections and assignments, line continuations inside operators, and a dozen smaller points. 0.9.1 fixed six alias cases that POSIX specifies: a here-document or a line continuation in an alias value, the `()` of a function definition from an alias, and the token that replaces one following an alias whose value ends in a blank.

### The 8 that fail on V86BSD

* **5 alias cases that POSIX leaves unspecified.** Four expect `in`, `do`, `for` or `case` to be replaced by aliases. POSIX says it is unspecified whether a word that could be a reserved word is subject to alias substitution. The fifth defines an alias inside a function and uses it in a command substitution in the same function, which depends on when that command substitution is parsed (also unspecified, see the next item).
* **2 cases that POSIX.1-2024 leaves unspecified.** POSIX does not say whether the commands of a command substitution are parsed all at once or line by line. The tests check for line by line; V86BSD, dash and bash parse them all at once.
* **1 `read` case** (`read.tst:203`): a deliberate choice that follows the 2024 text. POSIX.1-2024 gives the last variable the rest of the line "unsplit". So `IFS=' -' read a b c` of `A-B-C - ` sets `c` to `C -`; the other shells, and the test, give `C`.

One more case, `wait.tst:100`, passes or fails depending on timing: its expected status depends on whether the signal arrives while a child process is running. It passed in this run and failed in the one before; dash fails it too.

## The Open POSIX Test Suite

The suite comes from LTP (git 43bf9cf): 1888 tests, of which 1848 compile on V86BSD. The other 40 use interfaces that are not part of POSIX (`<sys/param.h>` 12, `cpu_set_t` 10, `RLIMIT_MEMLOCK` 5, `<features.h>` 2, `<sys/vfs.h>`, `<sys/sysinfo.h>`, `<mntent.h>`, `<getopt.h>`), ones that older editions had and V86BSD does not (`<ucontext.h>` 2, `ctime_r()`, `asctime_r()`), or a limit the test assumes exists (`SS_REPL_MAX`, 3: a defect in the test). The first run counted 111 that did not compile (made with 0.9); the second 117 (0.9.1 hid the `POLL_*` codes of `SIGPOLL`: six tests); the third 40, after `gettimeofday()`, `sighold()` and the other XSI interfaces of SUSv2 to SUSv4 were declared for `_XOPEN_SOURCE` 500 to 700 (`share/doc/legacy.md`).

| | passed |
|---|---|
| **V86BSD, on its kernel** | **1812 / 1848** |
| glibc on Linux, the same tests | 1693 |

36 of the tests that compile do not pass: 16 unsupported, 10 untested, 4 failed, 3 unresolved and 3 that crash with SIGSEGV. In the first run, where 36 did not pass, none was a fault in V86BSD, and 26 of them did not pass on glibc either; the details, test by test, are in `lib/libc/STATUS.md`, stage 17. The fourth run was not analysed test by test again, except for the three that crash and the three that began to compile and did not pass. The crashing ones, `pthread_cancel/5-1`, `pthread_detach/4-2` and `pthread_join/6-2`, use a thread ID after the thread has ended and been reclaimed, which POSIX leaves undefined; V86BSD unmaps the thread's stack at once, where glibc keeps the old thread's memory. Of the three that began to compile, two showed gaps of the kernel that are fixed now (with tests, `k_itimer` and `k_sigq`):

* `pthread_create/11-1` hung: `setitimer(ITIMER_VIRTUAL, ...)` was accepted but the timer never expired (only `ITIMER_REAL` was run by the clock tick), so the test's busy loop waited for a `SIGVTALRM` that never came. `ITIMER_VIRTUAL` and `ITIMER_PROF` now count the process's CPU time.
* `sigqueue/9-1`: `sysconf(_SC_SIGQUEUE_MAX)` reported the limit `RLIMIT_SIGPENDING` of 1024, but the kernel did not enforce it. It does now: the 1025th `sigqueue()` fails with `EAGAIN`.

The third is still open: `sigwaitinfo/6-1`. The signal that `raise()` sends has `si_code` `SI_TKILL` and the test wants `SI_USER` (glibc gives `SI_USER`). POSIX gives `si_code` for `kill()` only, and `SI_TKILL` is what Linux's `tkill` gives.

In outline the others are:

* features that are optional or whose limits are indeterminate (process contention scope, `AIO_MAX`, `SEM_NSEMS_MAX`);
* behaviour that is undefined, or a "may fail";
* defects in the tests;
* one test that takes more than 300 seconds under QEMU's emulation.

## Reproducing the results

* **os-test:** `sh lib/libc/tools/ost/vmrun.sh OBJDIR OSTEST WORKDIR` builds the system image from `make buildworld buildkernel OBJDIR=...`, runs os-test in V86BSD with a progress bar and makes the report. By hand: `lib/libc/tools/ost/README.md`.
* **POSIX Shell Tests:** `sh regress/bin/sh/tests/posix_shell_tests.sh SUITE SHELL` on a host with yash and `script(1)`.
* **Open POSIX Test Suite:** `sh lib/libc/tools/ptsrun.sh OBJDIR PTS WORKDIR` (PTS is LTP's `testcases/open_posix_testsuite`) runs it in V86BSD with a progress bar and sums up the results; by hand, `sh lib/libc/tools/pts.sh SRC OUT c17 [TIMEOUT [K/M]]` inside V86BSD.
