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

/* signal.h - signal numbers and structures (the ABI part; x86-64 layouts). */
#ifndef _SYS_SIGNAL_H_
#define _SYS_SIGNAL_H_

#include <sys/types.h>

#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGTRAP 5
#define SIGABRT 6
#define SIGBUS 7
#define SIGFPE 8
#define SIGKILL 9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGSTKFLT 16
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22
#define SIGURG 23
#define SIGXCPU 24
#define SIGXFSZ 25
#define SIGVTALRM 26
#define SIGPROF 27
#define SIGWINCH 28
#define SIGIO 29
#define SIGPWR 30
#define SIGSYS 31
#define SIGRTMIN 32
#define NSIG 64

#define SIG_DFL 0UL
#define SIG_IGN 1UL

#define SA_NOCLDSTOP 1UL
#define SA_NOCLDWAIT 2UL
#define SA_SIGINFO 4UL
#define SA_RESTORER 0x04000000UL
#define SA_ONSTACK 0x08000000UL
#define SA_RESTART 0x10000000UL
#define SA_NODEFER 0x40000000UL
#define SA_RESETHAND 0x80000000UL

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#define SS_ONSTACK 1
#define SS_DISABLE 2

/* si_code */
#define SI_USER 0
#define SI_KERNEL 0x80
#define SI_QUEUE -1
#define SI_TIMER -2
#define SI_MESGQ -3
#define SI_ASYNCIO -4
#define SI_TKILL -6
#define ILL_ILLOPN 2
#define FPE_INTDIV 1
#define FPE_FLTINV 7
#define SEGV_MAPERR 1
#define SEGV_ACCERR 2
#define BUS_ADRALN 1
#define BUS_ADRERR 2
#define CLD_EXITED 1
#define CLD_KILLED 2
#define CLD_DUMPED 3
#define CLD_TRAPPED 4
#define CLD_STOPPED 5
#define CLD_CONTINUED 6

typedef uint64_t sigset_t;
#define SIGBIT(s) (1UL << ((s) - 1))
#define UNBLOCKABLE (SIGBIT(SIGKILL) | SIGBIT(SIGSTOP))

/* siginfo_t: 128 bytes */
struct siginfo {
  int32_t signo, errno_, code, pad0;
  union {
    struct {
      int32_t pid;
      uint32_t uid;
      uint64_t value;
    } kill; /* also rt, queue */
    struct {
      int32_t tid, overrun;
      uint64_t value;
    } timer;
    struct {
      int32_t pid;
      uint32_t uid;
      int32_t status, pad;
      int64_t utime, stime;
    } chld;
    struct {
      uint64_t addr;
    } fault;
    struct {
      int64_t band;
      int32_t fd;
    } poll;
    uint8_t pad[112];
  } u;
};

struct sigaltstack {
  uint64_t sp;
  int32_t flags, pad;
  uint64_t size;
};

#endif
