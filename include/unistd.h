/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _UNISTD_H
#define _UNISTD_H
#include <bits/features.h>
#include <bits/null.h>
#include <bits/fcntl.h>
#define __NEED_size_t
#define __NEED_ssize_t
#define __NEED_uid_t
#define __NEED_gid_t
#define __NEED_off_t
#define __NEED_pid_t
#define __NEED_intptr_t
#define __NEED_useconds_t
#include <bits/alltypes.h>

/* ---- Versions ---- */
#define _POSIX_VERSION  202405L
#define _POSIX2_VERSION 202405L
#define _XOPEN_VERSION  800

/* ---- Options and option groups ---- */
#define _POSIX_ADVISORY_INFO            202405L
#define _POSIX_ASYNCHRONOUS_IO          202405L
#define _POSIX_BARRIERS                 202405L
#define _POSIX_CHOWN_RESTRICTED         1
#define _POSIX_CLOCK_SELECTION          202405L
#define _POSIX_CPUTIME                  202405L
#define _POSIX_DEVICE_CONTROL           202405L
#define _POSIX_FSYNC                    202405L
#define _POSIX_IPV6                     202405L
#define _POSIX_JOB_CONTROL              1
#define _POSIX_MAPPED_FILES             202405L
#define _POSIX_MEMLOCK                  202405L
#define _POSIX_MEMLOCK_RANGE            202405L
#define _POSIX_MEMORY_PROTECTION        202405L
#define _POSIX_MESSAGE_PASSING          202405L
#define _POSIX_MONOTONIC_CLOCK          202405L
#define _POSIX_NO_TRUNC                 1
#define _POSIX_PRIORITIZED_IO           202405L
#define _POSIX_PRIORITY_SCHEDULING      202405L
#define _POSIX_RAW_SOCKETS              202405L
#define _POSIX_READER_WRITER_LOCKS      202405L
#define _POSIX_REALTIME_SIGNALS         202405L
#define _POSIX_REGEXP                   1
#define _POSIX_SAVED_IDS                1
#define _POSIX_SEMAPHORES               202405L
#define _POSIX_SHARED_MEMORY_OBJECTS    202405L
#define _POSIX_SHELL                    1
#define _POSIX_SPAWN                    202405L
#define _POSIX_SPIN_LOCKS               202405L
#define _POSIX_SPORADIC_SERVER          0       /* sysconf() tells */
#define _POSIX_SYNCHRONIZED_IO          202405L
#define _POSIX_THREAD_ATTR_STACKADDR    202405L
#define _POSIX_THREAD_ATTR_STACKSIZE    202405L
#define _POSIX_THREAD_CPUTIME           202405L
#define _POSIX_THREAD_PRIO_INHERIT      202405L
#define _POSIX_THREAD_PRIO_PROTECT      202405L
#define _POSIX_THREAD_PRIORITY_SCHEDULING 202405L
#define _POSIX_THREAD_PROCESS_SHARED    202405L
#define _POSIX_THREAD_ROBUST_PRIO_INHERIT 202405L
#define _POSIX_THREAD_ROBUST_PRIO_PROTECT 202405L
#define _POSIX_THREAD_SAFE_FUNCTIONS    202405L
#define _POSIX_THREAD_SPORADIC_SERVER   0       /* sysconf() tells */
#define _POSIX_THREADS                  202405L
#define _POSIX_TIMEOUTS                 202405L
#define _POSIX_TIMERS                   202405L
#define _POSIX_TYPED_MEMORY_OBJECTS     0       /* sysconf() tells */
#define _POSIX_V8_ILP32_OFF32           (-1)
#define _POSIX_V8_ILP32_OFFBIG          (-1)
#define _POSIX_V8_LP64_OFF64            1
#define _POSIX_V8_LPBIG_OFFBIG          1       /* the same as LP64_OFF64 */
#define _POSIX_V7_ILP32_OFF32           (-1)
#define _POSIX_V7_ILP32_OFFBIG          (-1)
#define _POSIX_V7_LP64_OFF64            1
#define _POSIX_V7_LPBIG_OFFBIG          1
#define _POSIX2_C_BIND                  202405L
#define _POSIX2_C_DEV                   202405L
#define _POSIX2_CHAR_TERM               1
#define _POSIX2_FORT_RUN                202405L /* asa */
#define _POSIX2_LOCALEDEF               202405L
#define _POSIX2_SW_DEV                  202405L
#define _POSIX2_UPE                     202405L
#if __XSI_VISIBLE
#define _XOPEN_CRYPT                    1
#define _XOPEN_ENH_I18N                 1
#define _XOPEN_REALTIME                 1
#define _XOPEN_REALTIME_THREADS         1
#define _XOPEN_SHM                      1
#define _XOPEN_UNIX                     1
#endif
/* the UUCP Utilities option: [UU], not XSI */
#define _XOPEN_UUCP                     1

/* ---- Constants of older editions (share/doc/legacy.md) ---- */
#if __POSIX_RANGE(1L, 200112L)
#define _POSIX2_C_VERSION               199209L /* POSIX.2's C binding */
#endif
#if __POSIX_RANGE(1L, 202405L)
#define _POSIX2_FORT_DEV                (-1)    /* no FORTRAN compiler */
#endif
#if __XSI_OB5
#define _XOPEN_XCU_VERSION              5       /* SUSv2's XCU: Issue 5 */
#define _XOPEN_XPG2                     1
#define _XOPEN_XPG3                     1
#define _XOPEN_XPG4                     1
#endif
#if __XSI_OB6
#define _XOPEN_LEGACY                   1       /* the Legacy Feature Group */
#define _XBS5_ILP32_OFF32               (-1)
#define _XBS5_ILP32_OFFBIG              (-1)
#define _XBS5_LP64_OFF64                1
#define _XBS5_LPBIG_OFFBIG              1       /* the same as LP64_OFF64 */
#endif
#if __POSIX_RANGE(200112L, 202405L)
#define _POSIX_V6_ILP32_OFF32           (-1)
#define _POSIX_V6_ILP32_OFFBIG          (-1)
#define _POSIX_V6_LP64_OFF64            1
#define _POSIX_V6_LPBIG_OFFBIG          1
#define _POSIX_TRACE                    (-1)
#define _POSIX_TRACE_EVENT_FILTER       (-1)
#define _POSIX_TRACE_INHERIT            (-1)
#define _POSIX_TRACE_LOG                (-1)
#define _POSIX2_PBS                     (-1)
#define _POSIX2_PBS_ACCOUNTING          (-1)
#define _POSIX2_PBS_CHECKPOINT          (-1)
#define _POSIX2_PBS_LOCATE              (-1)
#define _POSIX2_PBS_MESSAGE             (-1)
#define _POSIX2_PBS_TRACK               (-1)
#endif
#if __EXT_VISIBLE || (__XSI_VISIBLE >= 600 && __XSI_VISIBLE < 800)
#define _XOPEN_STREAMS                  (-1)
#endif

/* ---- Execution-time constants ---- */
#define _POSIX_ASYNC_IO  1
#define _POSIX_PRIO_IO   1
#define _POSIX_SYNC_IO   1
#define _POSIX2_SYMLINKS 1
/* _POSIX_FALLOC and _POSIX_TIMESTAMP_RESOLUTION vary by file system. */

/* ---- Constants for functions ---- */
#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif
#define SEEK_DATA 3
#define SEEK_HOLE 4

#if __XSI_VISIBLE
#define F_ULOCK 0
#define F_LOCK  1
#define F_TLOCK 2
#define F_TEST  3
#endif

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define _POSIX_VDISABLE '\0'
#define POSIX_CLOSE_RESTART 0

#define _CS_PATH                              0
#define _CS_POSIX_V8_ILP32_OFF32_CFLAGS       1
#define _CS_POSIX_V8_ILP32_OFF32_LDFLAGS      2
#define _CS_POSIX_V8_ILP32_OFF32_LIBS         3
#define _CS_POSIX_V8_ILP32_OFFBIG_CFLAGS      4
#define _CS_POSIX_V8_ILP32_OFFBIG_LDFLAGS     5
#define _CS_POSIX_V8_ILP32_OFFBIG_LIBS        6
#define _CS_POSIX_V8_LP64_OFF64_CFLAGS        7
#define _CS_POSIX_V8_LP64_OFF64_LDFLAGS       8
#define _CS_POSIX_V8_LP64_OFF64_LIBS          9
#define _CS_POSIX_V8_LPBIG_OFFBIG_CFLAGS      10
#define _CS_POSIX_V8_LPBIG_OFFBIG_LDFLAGS     11
#define _CS_POSIX_V8_LPBIG_OFFBIG_LIBS        12
#define _CS_POSIX_V8_THREADS_CFLAGS           13
#define _CS_POSIX_V8_THREADS_LDFLAGS          14
#define _CS_POSIX_V8_WIDTH_RESTRICTED_ENVS    15
#define _CS_V8_ENV                            16
#define _CS_POSIX_V7_ILP32_OFF32_CFLAGS       17
#define _CS_POSIX_V7_ILP32_OFF32_LDFLAGS      18
#define _CS_POSIX_V7_ILP32_OFF32_LIBS         19
#define _CS_POSIX_V7_ILP32_OFFBIG_CFLAGS      20
#define _CS_POSIX_V7_ILP32_OFFBIG_LDFLAGS     21
#define _CS_POSIX_V7_ILP32_OFFBIG_LIBS        22
#define _CS_POSIX_V7_LP64_OFF64_CFLAGS        23
#define _CS_POSIX_V7_LP64_OFF64_LDFLAGS       24
#define _CS_POSIX_V7_LP64_OFF64_LIBS          25
#define _CS_POSIX_V7_LPBIG_OFFBIG_CFLAGS      26
#define _CS_POSIX_V7_LPBIG_OFFBIG_LDFLAGS     27
#define _CS_POSIX_V7_LPBIG_OFFBIG_LIBS        28
#define _CS_POSIX_V7_THREADS_CFLAGS           29
#define _CS_POSIX_V7_THREADS_LDFLAGS          30
#define _CS_POSIX_V7_WIDTH_RESTRICTED_ENVS    31
#define _CS_V7_ENV                            32
#if __XSI_OB6
#define _CS_XBS5_ILP32_OFF32_CFLAGS           33
#define _CS_XBS5_ILP32_OFF32_LDFLAGS          34
#define _CS_XBS5_ILP32_OFF32_LIBS             35
#define _CS_XBS5_ILP32_OFF32_LINTFLAGS        36
#define _CS_XBS5_ILP32_OFFBIG_CFLAGS          37
#define _CS_XBS5_ILP32_OFFBIG_LDFLAGS         38
#define _CS_XBS5_ILP32_OFFBIG_LIBS            39
#define _CS_XBS5_ILP32_OFFBIG_LINTFLAGS       40
#define _CS_XBS5_LP64_OFF64_CFLAGS            41
#define _CS_XBS5_LP64_OFF64_LDFLAGS           42
#define _CS_XBS5_LP64_OFF64_LIBS              43
#define _CS_XBS5_LP64_OFF64_LINTFLAGS         44
#define _CS_XBS5_LPBIG_OFFBIG_CFLAGS          45
#define _CS_XBS5_LPBIG_OFFBIG_LDFLAGS         46
#define _CS_XBS5_LPBIG_OFFBIG_LIBS            47
#define _CS_XBS5_LPBIG_OFFBIG_LINTFLAGS       48
#endif
#if __POSIX_RANGE(200112L, 202405L)
#define _CS_POSIX_V6_ILP32_OFF32_CFLAGS       49
#define _CS_POSIX_V6_ILP32_OFF32_LDFLAGS      50
#define _CS_POSIX_V6_ILP32_OFF32_LIBS         51
#define _CS_POSIX_V6_ILP32_OFFBIG_CFLAGS      52
#define _CS_POSIX_V6_ILP32_OFFBIG_LDFLAGS     53
#define _CS_POSIX_V6_ILP32_OFFBIG_LIBS        54
#define _CS_POSIX_V6_LP64_OFF64_CFLAGS        55
#define _CS_POSIX_V6_LP64_OFF64_LDFLAGS       56
#define _CS_POSIX_V6_LP64_OFF64_LIBS          57
#define _CS_POSIX_V6_LPBIG_OFFBIG_CFLAGS      58
#define _CS_POSIX_V6_LPBIG_OFFBIG_LDFLAGS     59
#define _CS_POSIX_V6_LPBIG_OFFBIG_LIBS        60
#define _CS_POSIX_V6_WIDTH_RESTRICTED_ENVS    61
#endif
#if __POSIX_RANGE(200809L, 202405L)
#define _CS_V6_ENV                            62
#endif

#define _PC_2_SYMLINKS             0
#define _PC_ALLOC_SIZE_MIN         1
#define _PC_ASYNC_IO               2
#define _PC_CHOWN_RESTRICTED       3
#define _PC_FALLOC                 4
#define _PC_FILESIZEBITS           5
#define _PC_LINK_MAX               6
#define _PC_MAX_CANON              7
#define _PC_MAX_INPUT              8
#define _PC_NAME_MAX               9
#define _PC_NO_TRUNC               10
#define _PC_PATH_MAX               11
#define _PC_PIPE_BUF               12
#define _PC_PRIO_IO                13
#define _PC_REC_INCR_XFER_SIZE     14
#define _PC_REC_MAX_XFER_SIZE      15
#define _PC_REC_MIN_XFER_SIZE      16
#define _PC_REC_XFER_ALIGN         17
#define _PC_SYMLINK_MAX            18
#define _PC_SYNC_IO                19
#define _PC_TEXTDOMAIN_MAX         20
#define _PC_TIMESTAMP_RESOLUTION   21
#define _PC_VDISABLE               22

#define _SC_2_C_BIND                    0
#define _SC_2_C_DEV                     1
#define _SC_2_CHAR_TERM                 2
#define _SC_2_FORT_RUN                  3
#define _SC_2_LOCALEDEF                 4
#define _SC_2_SW_DEV                    5
#define _SC_2_UPE                       6
#define _SC_2_VERSION                   7
#define _SC_ADVISORY_INFO               8
#define _SC_AIO_LISTIO_MAX              9
#define _SC_AIO_MAX                     10
#define _SC_AIO_PRIO_DELTA_MAX          11
#define _SC_ARG_MAX                     12
#define _SC_ASYNCHRONOUS_IO             13
#define _SC_ATEXIT_MAX                  14
#define _SC_BARRIERS                    15
#define _SC_BC_BASE_MAX                 16
#define _SC_BC_DIM_MAX                  17
#define _SC_BC_SCALE_MAX                18
#define _SC_BC_STRING_MAX               19
#define _SC_CHILD_MAX                   20
#define _SC_CLK_TCK                     21
#define _SC_CLOCK_SELECTION             22
#define _SC_COLL_WEIGHTS_MAX            23
#define _SC_CPUTIME                     24
#define _SC_DELAYTIMER_MAX              25
#define _SC_DEVICE_CONTROL              26
#define _SC_EXPR_NEST_MAX               27
#define _SC_FSYNC                       28
#define _SC_GETGR_R_SIZE_MAX            29
#define _SC_GETPW_R_SIZE_MAX            30
#define _SC_HOST_NAME_MAX               31
#define _SC_IOV_MAX                     32
#define _SC_IPV6                        33
#define _SC_JOB_CONTROL                 34
#define _SC_LINE_MAX                    35
#define _SC_LOGIN_NAME_MAX              36
#define _SC_MAPPED_FILES                37
#define _SC_MEMLOCK                     38
#define _SC_MEMLOCK_RANGE               39
#define _SC_MEMORY_PROTECTION           40
#define _SC_MESSAGE_PASSING             41
#define _SC_MONOTONIC_CLOCK             42
#define _SC_MQ_OPEN_MAX                 43
#define _SC_MQ_PRIO_MAX                 44
#define _SC_NGROUPS_MAX                 45
#define _SC_NPROCESSORS_CONF            46
#define _SC_NPROCESSORS_ONLN            47
#define _SC_NSIG                        48
#define _SC_OPEN_MAX                    49
#define _SC_PAGE_SIZE                   50
#define _SC_PAGESIZE                    _SC_PAGE_SIZE
#define _SC_PRIORITIZED_IO              51
#define _SC_PRIORITY_SCHEDULING         52
#define _SC_RAW_SOCKETS                 53
#define _SC_RE_DUP_MAX                  54
#define _SC_READER_WRITER_LOCKS         55
#define _SC_REALTIME_SIGNALS            56
#define _SC_REGEXP                      57
#define _SC_RTSIG_MAX                   58
#define _SC_SAVED_IDS                   59
#define _SC_SEM_NSEMS_MAX               60
#define _SC_SEM_VALUE_MAX               61
#define _SC_SEMAPHORES                  62
#define _SC_SHARED_MEMORY_OBJECTS       63
#define _SC_SHELL                       64
#define _SC_SIGQUEUE_MAX                65
#define _SC_SPAWN                       66
#define _SC_SPIN_LOCKS                  67
#define _SC_SPORADIC_SERVER             68
#define _SC_SS_REPL_MAX                 69
#define _SC_STREAM_MAX                  70
#define _SC_SYMLOOP_MAX                 71
#define _SC_SYNCHRONIZED_IO             72
#define _SC_THREAD_ATTR_STACKADDR       73
#define _SC_THREAD_ATTR_STACKSIZE       74
#define _SC_THREAD_CPUTIME              75
#define _SC_THREAD_DESTRUCTOR_ITERATIONS 76
#define _SC_THREAD_KEYS_MAX             77
#define _SC_THREAD_PRIO_INHERIT         78
#define _SC_THREAD_PRIO_PROTECT         79
#define _SC_THREAD_PRIORITY_SCHEDULING  80
#define _SC_THREAD_PROCESS_SHARED       81
#define _SC_THREAD_ROBUST_PRIO_INHERIT  82
#define _SC_THREAD_ROBUST_PRIO_PROTECT  83
#define _SC_THREAD_SAFE_FUNCTIONS       84
#define _SC_THREAD_SPORADIC_SERVER      85
#define _SC_THREAD_STACK_MIN            86
#define _SC_THREAD_THREADS_MAX          87
#define _SC_THREADS                     88
#define _SC_TIMEOUTS                    89
#define _SC_TIMER_MAX                   90
#define _SC_TIMERS                      91
#define _SC_TTY_NAME_MAX                92
#define _SC_TYPED_MEMORY_OBJECTS        93
#define _SC_TZNAME_MAX                  94
#define _SC_V8_ILP32_OFF32              95
#define _SC_V8_ILP32_OFFBIG             96
#define _SC_V8_LP64_OFF64               97
#define _SC_V8_LPBIG_OFFBIG             98
#define _SC_V7_ILP32_OFF32              99
#define _SC_V7_ILP32_OFFBIG             100
#define _SC_V7_LP64_OFF64               101
#define _SC_V7_LPBIG_OFFBIG             102
#define _SC_VERSION                     103
#define _SC_XOPEN_CRYPT                 104
#define _SC_XOPEN_ENH_I18N              105
#define _SC_XOPEN_REALTIME              106
#define _SC_XOPEN_REALTIME_THREADS      107
#define _SC_XOPEN_SHM                   108
#define _SC_XOPEN_UNIX                  109
#define _SC_XOPEN_UUCP                  110
#define _SC_XOPEN_VERSION               111
#if __POSIX_RANGE(1L, 200112L)
#define _SC_2_C_VERSION                 112
#endif
#if __XSI_OB5
#define _SC_PASS_MAX                    113
#define _SC_XOPEN_XCU_VERSION           114
#endif
#if __XSI_OB6
#define _SC_XBS5_ILP32_OFF32            115
#define _SC_XBS5_ILP32_OFFBIG           116
#define _SC_XBS5_LP64_OFF64             117
#define _SC_XBS5_LPBIG_OFFBIG           118
#define _SC_XOPEN_LEGACY                119
#endif
#if __POSIX_RANGE(1L, 202405L)
#define _SC_2_FORT_DEV                  120
#endif
#if __POSIX_RANGE(200112L, 202405L)
#define _SC_V6_ILP32_OFF32              121
#define _SC_V6_ILP32_OFFBIG             122
#define _SC_V6_LP64_OFF64               123
#define _SC_V6_LPBIG_OFFBIG             124
#define _SC_2_PBS                       125
#define _SC_2_PBS_ACCOUNTING            126
#define _SC_2_PBS_CHECKPOINT            127
#define _SC_2_PBS_LOCATE                128
#define _SC_2_PBS_MESSAGE               129
#define _SC_2_PBS_TRACK                 130
#define _SC_TRACE                       131
#define _SC_TRACE_EVENT_FILTER          132
#define _SC_TRACE_EVENT_NAME_MAX        133
#define _SC_TRACE_INHERIT               134
#define _SC_TRACE_LOG                   135
#define _SC_TRACE_NAME_MAX              136
#define _SC_TRACE_SYS_MAX               137
#define _SC_TRACE_USER_EVENT_MAX        138
#endif
#if __EXT_VISIBLE || (__XSI_VISIBLE >= 600 && __XSI_VISIBLE < 800)
#define _SC_XOPEN_STREAMS               139
#endif

__BEGIN_DECLS
int          access(const char *, int);
unsigned     alarm(unsigned);
int          chdir(const char *);
int          chown(const char *, uid_t, gid_t);
int          close(int);
size_t       confstr(int, char *, size_t);
int          dup(int);
int          dup2(int, int);
int          dup3(int, int, int);
__NORETURN void _exit(int);
int          execl(const char *, const char *, ...);
int          execle(const char *, const char *, ...);
int          execlp(const char *, const char *, ...);
int          execv(const char *, char *const []);
int          execve(const char *, char *const [], char *const []);
int          execvp(const char *, char *const []);
int          faccessat(int, const char *, int, int);
int          fchdir(int);
int          fchown(int, uid_t, gid_t);
int          fchownat(int, const char *, uid_t, gid_t, int);
int          fdatasync(int);
int          fexecve(int, char *const [], char *const []);
pid_t        _Fork(void);
pid_t        fork(void);
long         fpathconf(int, int);
int          fsync(int);
int          ftruncate(int, off_t);
char        *getcwd(char *, size_t);
gid_t        getegid(void);
int          getentropy(void *, size_t);
uid_t        geteuid(void);
gid_t        getgid(void);
int          getgroups(int, gid_t []);
int          gethostname(char *, size_t);
int          getopt(int, char *const [], const char *);
pid_t        getpgid(pid_t);
pid_t        getpgrp(void);
pid_t        getpid(void);
pid_t        getppid(void);
pid_t        getsid(pid_t);
uid_t        getuid(void);
int          isatty(int);
int          lchown(const char *, uid_t, gid_t);
int          link(const char *, const char *);
int          linkat(int, const char *, int, const char *, int);
off_t        lseek(int, off_t, int);
long         pathconf(const char *, int);
int          pause(void);
int          pipe(int [2]);
int          pipe2(int [2], int);
int          posix_close(int, int);
ssize_t      pread(int, void *, size_t, off_t);
ssize_t      pwrite(int, const void *, size_t, off_t);
ssize_t      read(int, void *, size_t);
ssize_t      readlink(const char *__RESTRICT, char *__RESTRICT, size_t);
ssize_t      readlinkat(int, const char *__RESTRICT, char *__RESTRICT, size_t);
int          rmdir(const char *);
int          setegid(gid_t);
int          seteuid(uid_t);
int          setgid(gid_t);
int          setpgid(pid_t, pid_t);
int          setuid(uid_t);
pid_t        setsid(void);
unsigned     sleep(unsigned);
int          symlink(const char *, const char *);
int          symlinkat(const char *, int, const char *);
long         sysconf(int);
pid_t        tcgetpgrp(int);
int          tcsetpgrp(int, pid_t);
int          truncate(const char *, off_t);
char        *ttyname(int);
int          ttyname_r(int, char *, size_t);
int          unlink(const char *);
int          unlinkat(int, const char *, int);
ssize_t      write(int, const void *, size_t);
#if __XSI_VISIBLE
long         gethostid(void);
int          getresgid(gid_t *__RESTRICT, gid_t *__RESTRICT, gid_t *__RESTRICT);
int          getresuid(uid_t *__RESTRICT, uid_t *__RESTRICT, uid_t *__RESTRICT);
int          lockf(int, int, off_t);
int          nice(int);
int          setregid(gid_t, gid_t);
int          setresgid(gid_t, gid_t, gid_t);
int          setresuid(uid_t, uid_t, uid_t);
int          setreuid(uid_t, uid_t);
void         swab(const void *__RESTRICT, void *__RESTRICT, ssize_t);
void         sync(void);
#endif
#if __POSIX_VISIBLE
char        *getlogin(void);
int          getlogin_r(char *, size_t);
#endif
#if __XSI_VISIBLE
char        *crypt(const char *, const char *);
void         encrypt(char [64], int);
#endif
#if __XSI_OB
pid_t        setpgrp(void);                /* SUSv2-SUSv4, not POSIX.1-2024 */
#endif
#if __XSI_OB6
/* SUSv2, SUSv3 only (LEGACY or obsolescent there) */
char        *getwd(char *);
useconds_t   ualarm(useconds_t, useconds_t);
int          usleep(useconds_t);
pid_t        vfork(void);
#endif
#if __XSI_OB5
/* SUSv2 only (LEGACY) */
int          brk(void *);
void        *sbrk(intptr_t);
int          chroot(const char *);
char        *ctermid(char *);
char        *cuserid(char *);
int          getdtablesize(void);
int          getpagesize(void);
char        *getpass(const char *);
#endif
#if __EXT_VISIBLE
int          revoke(const char *);
int          sethostname(const char *, size_t);
#endif

extern char *optarg;
extern int opterr, optind, optopt;
__END_DECLS
#endif
