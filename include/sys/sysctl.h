/*
 * Copyright (c) 2026, asprintf
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

#ifndef _SYS_SYSCTL_H
#define _SYS_SYSCTL_H
#include <bits/features.h>
#define __NEED_size_t
#include <bits/alltypes.h>

/* sysctl.h - sysctl(3): the system's variables, named by integers or by
 * name (the numbers are FreeBSD's, from its sys/sys/sysctl.h).  The kernel
 * answers the other trees of the names (kern.proc, net.route ...) with its
 * own structures (<sys/user.h>, <net/route.h>). */

#define CTL_MAXNAME 24

#define CTL_MAXNAME 24
#define CTLTYPE_NODE 1
#define CTLTYPE_INT 2
#define CTLTYPE_STRING 3
#define CTLTYPE_S64 4
#define CTLTYPE_OPAQUE 5
#define CTLTYPE_STRUCT CTLTYPE_OPAQUE
#define CTLTYPE_UINT 6
#define CTLTYPE_LONG 7
#define CTLTYPE_ULONG 8
#define CTLTYPE_U64 9
#define CTLTYPE_U8 0xa
#define CTLTYPE_U16 0xb
#define CTLTYPE_S8 0xc
#define CTLTYPE_S16 0xd
#define CTLTYPE_S32 0xe
#define CTLTYPE_U32 0xf
#define CTLFLAG_RD 0x80000000
#define CTLFLAG_WR 0x40000000
#define CTLFLAG_RW (CTLFLAG_RD|CTLFLAG_WR)
#define CTLFLAG_DORMANT 0x20000000
#define CTLFLAG_ANYBODY 0x10000000
#define CTLFLAG_SECURE 0x08000000
#define CTLFLAG_PRISON 0x04000000
#define CTLFLAG_DYN 0x02000000
#define CTLFLAG_SKIP 0x01000000
#define CTLMASK_SECURE 0x00F00000
#define CTLFLAG_TUN 0x00080000
#define CTLFLAG_RDTUN (CTLFLAG_RD|CTLFLAG_TUN)
#define CTLFLAG_RWTUN (CTLFLAG_RW|CTLFLAG_TUN)
#define CTLFLAG_MPSAFE 0x00040000
#define CTLFLAG_VNET 0x00020000
#define CTLFLAG_DYING 0x00010000
#define CTLFLAG_CAPRD 0x00008000
#define CTLFLAG_CAPWR 0x00004000
#define CTLFLAG_STATS 0x00002000
#define CTLFLAG_NOFETCH 0x00001000
#define CTLFLAG_CAPRW (CTLFLAG_CAPRD|CTLFLAG_CAPWR)
#define CTLFLAG_NEEDGIANT 0x00000800
#define CTLFLAG_SECURE1 (CTLFLAG_SECURE | (0 << CTLSHIFT_SECURE))
#define CTLFLAG_SECURE2 (CTLFLAG_SECURE | (1 << CTLSHIFT_SECURE))
#define CTLFLAG_SECURE3 (CTLFLAG_SECURE | (2 << CTLSHIFT_SECURE))
#define CTL_AUTO_START 0x100
#define CTL_SYSCTL 0
#define CTL_KERN 1
#define CTL_VM 2
#define CTL_VFS 3
#define CTL_NET 4
#define CTL_DEBUG 5
#define CTL_HW 6
#define CTL_MACHDEP 7
#define CTL_USER 8
#define CTL_P1003_1B 9
#define CTL_SYSCTL_DEBUG 0
#define CTL_SYSCTL_NAME 1
#define CTL_SYSCTL_NEXT 2
#define CTL_SYSCTL_NAME2OID 3
#define CTL_SYSCTL_OIDFMT 4
#define CTL_SYSCTL_OIDDESCR 5
#define CTL_SYSCTL_OIDLABEL 6
#define CTL_SYSCTL_NEXTNOSKIP 7
#define KERN_OSTYPE 1
#define KERN_OSRELEASE 2
#define KERN_OSREV 3
#define KERN_VERSION 4
#define KERN_MAXVNODES 5
#define KERN_MAXPROC 6
#define KERN_MAXFILES 7
#define KERN_ARGMAX 8
#define KERN_SECURELVL 9
#define KERN_HOSTNAME 10
#define KERN_HOSTID 11
#define KERN_CLOCKRATE 12
#define KERN_PROC 14
#define KERN_FILE 15
#define KERN_PROF 16
#define KERN_POSIX1 17
#define KERN_NGROUPS 18
#define KERN_JOB_CONTROL 19
#define KERN_SAVED_IDS 20
#define KERN_BOOTTIME 21
#define KERN_NISDOMAINNAME 22
#define KERN_UPDATEINTERVAL 23
#define KERN_OSRELDATE 24
#define KERN_NTP_PLL 25
#define KERN_BOOTFILE 26
#define KERN_MAXFILESPERPROC 27
#define KERN_MAXPROCPERUID 28
#define KERN_DUMPDEV 29
#define KERN_IPC 30
#define KERN_DUMMY 31
#define KERN_PS_STRINGS 32
#define KERN_USRSTACK 33
#define KERN_LOGSIGEXIT 34
#define KERN_IOV_MAX 35
#define KERN_HOSTUUID 36
#define KERN_ARND 37
#define KERN_MAXPHYS 38
#define KERN_LOCKF 39
#define KERN_PROC_ALL 0
#define KERN_PROC_PID 1
#define KERN_PROC_PGRP 2
#define KERN_PROC_SESSION 3
#define KERN_PROC_TTY 4
#define KERN_PROC_UID 5
#define KERN_PROC_RUID 6
#define KERN_PROC_ARGS 7
#define KERN_PROC_PROC 8
#define KERN_PROC_SV_NAME 9
#define KERN_PROC_RGID 10
#define KERN_PROC_GID 11
#define KERN_PROC_PATHNAME 12
#define KERN_PROC_OVMMAP 13
#define KERN_PROC_OFILEDESC 14
#define KERN_PROC_KSTACK 15
#define KERN_PROC_INC_THREAD 0x10
#define KERN_PROC_VMMAP 32
#define KERN_PROC_FILEDESC 33
#define KERN_PROC_GROUPS 34
#define KERN_PROC_ENV 35
#define KERN_PROC_AUXV 36
#define KERN_PROC_RLIMIT 37
#define KERN_PROC_PS_STRINGS 38
#define KERN_PROC_UMASK 39
#define KERN_PROC_OSREL 40
#define KERN_PROC_SIGTRAMP 41
#define KERN_PROC_CWD 42
#define KERN_PROC_NFDS 43
#define KERN_PROC_SIGFASTBLK 44
#define KERN_PROC_VM_LAYOUT 45
#define KERN_PROC_RLIMIT_USAGE 46
#define KERN_PROC_KQUEUE 47
#define HW_MACHINE 1
#define HW_MODEL 2
#define HW_NCPU 3
#define HW_BYTEORDER 4
#define HW_PHYSMEM 5
#define HW_USERMEM 6
#define HW_PAGESIZE 7
#define HW_DISKNAMES 8
#define HW_DISKSTATS 9
#define HW_FLOATINGPT 10
#define HW_MACHINE_ARCH 11
#define HW_REALMEM 12
#define CTL_P1003_1B_ASYNCHRONOUS_IO 1
#define CTL_P1003_1B_MAPPED_FILES 2
#define CTL_P1003_1B_MEMLOCK 3
#define CTL_P1003_1B_MEMLOCK_RANGE 4
#define CTL_P1003_1B_MEMORY_PROTECTION 5
#define CTL_P1003_1B_MESSAGE_PASSING 6
#define CTL_P1003_1B_PRIORITIZED_IO 7
#define CTL_P1003_1B_PRIORITY_SCHEDULING 8
#define CTL_P1003_1B_REALTIME_SIGNALS 9
#define CTL_P1003_1B_SEMAPHORES 10
#define CTL_P1003_1B_FSYNC 11
#define CTL_P1003_1B_SHARED_MEMORY_OBJECTS 12
#define CTL_P1003_1B_SYNCHRONIZED_IO 13
#define CTL_P1003_1B_TIMERS 14
#define CTL_P1003_1B_AIO_LISTIO_MAX 15
#define CTL_P1003_1B_AIO_MAX 16
#define CTL_P1003_1B_AIO_PRIO_DELTA_MAX 17
#define CTL_P1003_1B_DELAYTIMER_MAX 18
#define CTL_P1003_1B_MQ_OPEN_MAX 19
#define CTL_P1003_1B_PAGESIZE 20
#define CTL_P1003_1B_RTSIG_MAX 21
#define CTL_P1003_1B_SEM_NSEMS_MAX 22
#define CTL_P1003_1B_SEM_VALUE_MAX 23
#define CTL_P1003_1B_SIGQUEUE_MAX 24
#define CTL_P1003_1B_TIMER_MAX 25
#define CTL_P1003_1B_MAXID 26

__BEGIN_DECLS
int sysctl(const int *, unsigned int, void *, size_t *, const void *, size_t);
int sysctlbyname(const char *, void *, size_t *, const void *, size_t);
int sysctlnametomib(const char *, int *, size_t *);
__END_DECLS
#endif
