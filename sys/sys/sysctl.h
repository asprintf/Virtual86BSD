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
#define __NEED_size_t
#include <bits/alltypes.h>

/* sysctl.h - sysctl(3): the system's variables, named by integers.
 *
 * The names are a path of integers, like a file name: CTL_KERN, KERN_OSTYPE
 * is "kern.ostype".  The numbers are those of OpenBSD, so that a program
 * that uses them there uses them here; every number in OpenBSD's tree is
 * reserved, but the kernel (kern/kern_sysctl.c) answers only for some of
 * them, the others fail with EOPNOTSUPP, and `sysctl -a' lists what answers.
 * The CTL_*_NAMES tables are indexed by the number and are what sysctl(8)
 * walks to turn a name into a path.  What is not here: the structures of
 * the nodes that nothing exports yet (kinfo_proc, kinfo_file ...), the
 * sub-trees below kern.proc, kern.file, hw.sensors, hw.battery, kern.tty,
 * kern.malloc and the routing tables, and the net.* nodes of families
 * other than unix and inet.
 */

#define CTL_MAXNAME 12 /* the longest path */

/* a node of the tree: its name and what it is */
struct ctlname {
	const char *ctl_name;
	int ctl_type;
};
#define CTLTYPE_NODE   1 /* a node: names below it */
#define CTLTYPE_INT    2 /* an int */
#define CTLTYPE_STRING 3 /* a string, with its terminating NUL counted */
#define CTLTYPE_QUAD   4 /* a 64-bit number */
#define CTLTYPE_STRUCT 5 /* a structure */

/* the top level */
#define CTL_UNSPEC  0
#define CTL_KERN    1  /* the kernel's variables */
#define CTL_VM      2  /* virtual memory */
/* 3 was CTL_FS */
#define CTL_NET     4  /* networking: family, protocol, variable */
#define CTL_DEBUG   5  /* debugging variables */
#define CTL_HW      6  /* hardware */
#define CTL_MACHDEP 7  /* machine dependent */
/* 8 was CTL_USER */
#define CTL_DDB     9  /* the debugger */
#define CTL_VFS     10 /* file systems */
#define CTL_MAXID   11

#define CTL_NAMES { \
	{ 0, 0 }, \
	{ "kern", CTLTYPE_NODE }, \
	{ "vm", CTLTYPE_NODE }, \
	{ "gap", 0 }, \
	{ "net", CTLTYPE_NODE }, \
	{ "debug", CTLTYPE_NODE }, \
	{ "hw", CTLTYPE_NODE }, \
	{ "machdep", CTLTYPE_NODE }, \
	{ "gap", 0 }, \
	{ "ddb", CTLTYPE_NODE }, \
	{ "vfs", CTLTYPE_NODE }, \
}

/* ---- kern ---- */
#define KERN_OSTYPE            1  /* string: the operating system's name */
#define KERN_OSRELEASE         2  /* string: its release */
#define KERN_OSREV             3  /* int: its revision */
#define KERN_VERSION           4  /* string: the kernel's banner */
#define KERN_MAXVNODES         5  /* int */
#define KERN_MAXPROC           6  /* int: most processes */
#define KERN_MAXFILES          7  /* int: most open files */
#define KERN_ARGMAX            8  /* int: most bytes of exec arguments */
#define KERN_SECURELVL         9  /* int */
#define KERN_HOSTNAME          10 /* string: the host name (read, write) */
#define KERN_HOSTID            11 /* int */
#define KERN_CLOCKRATE         12 /* struct clockinfo */
/* 13, 14, 15 were KERN_DNSJACKPORT, KERN_PROC, KERN_FILE */
#define KERN_PROF              16 /* node: profiling */
#define KERN_POSIX1            17 /* int: the POSIX.1 version */
#define KERN_NGROUPS           18 /* int: most supplementary groups */
#define KERN_JOB_CONTROL       19 /* int: job control is there */
#define KERN_SAVED_IDS         20 /* int: saved set-user-ID is there */
#define KERN_BOOTTIME          21 /* struct timeval: when the kernel started */
#define KERN_DOMAINNAME        22 /* string: the domain name (read, write) */
#define KERN_MAXPARTITIONS     23 /* int */
#define KERN_RAWPARTITION      24 /* int */
#define KERN_MAXTHREAD         25 /* int: most threads */
#define KERN_NTHREADS          26 /* int: threads now */
#define KERN_OSVERSION         27 /* string: the kernel's configuration and build */
#define KERN_SOMAXCONN         28 /* int */
#define KERN_SOMINCONN         29 /* int */
/* 30, 31 were KERN_USERMOUNT, KERN_RND */
#define KERN_NOSUIDCOREDUMP    32 /* int */
#define KERN_FSYNC             33 /* int */
#define KERN_SYSVMSG           34 /* int */
#define KERN_SYSVSEM           35 /* int */
#define KERN_SYSVSHM           36 /* int */
/* 37 was KERN_ARND */
#define KERN_MSGBUFSIZE        38 /* int */
#define KERN_MALLOCSTATS       39 /* node */
#define KERN_CPTIME            40 /* array */
#define KERN_NCHSTATS          41 /* struct */
#define KERN_FORKSTAT          42 /* struct */
/* 43 was KERN_NSELCOLL */
#define KERN_TTY               44 /* node */
#define KERN_CCPU              45 /* int */
#define KERN_FSCALE            46 /* int: the scale of fixed-point numbers */
#define KERN_NPROCS            47 /* int: processes now */
#define KERN_MSGBUF            48
#define KERN_POOL              49 /* node */
#define KERN_STACKGAPRANDOM    50 /* int */
#define KERN_SYSVIPC_INFO      51 /* struct */
#define KERN_ALLOWKMEM         52 /* int */
#define KERN_WITNESSWATCH      53 /* int */
#define KERN_SPLASSERT         54 /* int */
#define KERN_PROC_ARGS         55 /* node */
#define KERN_NFILES            56 /* int: open files now */
#define KERN_TTYCOUNT          57 /* int */
#define KERN_NUMVNODES         58 /* int */
#define KERN_MBSTAT            59 /* struct */
#define KERN_WITNESS           60 /* node */
#define KERN_SEMINFO           61 /* struct */
#define KERN_SHMINFO           62 /* struct */
#define KERN_INTRCNT           63 /* node */
#define KERN_WATCHDOG          64 /* node */
#define KERN_ALLOWDT           65 /* int */
#define KERN_PROC              66 /* struct: the processes */
#define KERN_MAXCLUSTERS       67 /* int */
#define KERN_EVCOUNT           68 /* node */
#define KERN_TIMECOUNTER       69 /* node */
#define KERN_MAXLOCKSPERUID    70 /* int */
#define KERN_CPTIME2           71 /* array */
#define KERN_CACHEPCT          72 /* int */
#define KERN_FILE              73 /* struct: the open files */
#define KERN_WXABORT           74 /* int */
#define KERN_CONSDEV           75 /* dev_t: the console */
#define KERN_NETLIVELOCKS      76 /* int */
#define KERN_POOL_DEBUG        77 /* int */
#define KERN_PROC_CWD          78 /* node */
#define KERN_PROC_NOBROADCASTKILL 79 /* node */
#define KERN_PROC_VMMAP        80 /* node */
#define KERN_GLOBAL_PTRACE     81 /* int */
#define KERN_CONSBUFSIZE       82 /* int */
#define KERN_CONSBUF           83
#define KERN_AUDIO             84 /* struct */
#define KERN_CPUSTATS          85 /* struct */
#define KERN_PFSTATUS          86 /* struct */
#define KERN_TIMEOUT_STATS     87 /* struct */
#define KERN_UTC_OFFSET        88 /* int */
#define KERN_VIDEO             89 /* struct */
#define KERN_CLOCKINTR         90 /* node */
#define KERN_AUTOCONF_SERIAL   91 /* int */
#define KERN_MAXID             92

#define CTL_KERN_NAMES { \
	{ 0, 0 }, \
	{ "ostype", CTLTYPE_STRING }, \
	{ "osrelease", CTLTYPE_STRING }, \
	{ "osrevision", CTLTYPE_INT }, \
	{ "version", CTLTYPE_STRING }, \
	{ "maxvnodes", CTLTYPE_INT }, \
	{ "maxproc", CTLTYPE_INT }, \
	{ "maxfiles", CTLTYPE_INT }, \
	{ "argmax", CTLTYPE_INT }, \
	{ "securelevel", CTLTYPE_INT }, \
	{ "hostname", CTLTYPE_STRING }, \
	{ "hostid", CTLTYPE_INT }, \
	{ "clockrate", CTLTYPE_STRUCT }, \
	{ "gap", 0 }, \
	{ "gap", 0 }, \
	{ "gap", 0 }, \
	{ "profiling", CTLTYPE_NODE }, \
	{ "posix1version", CTLTYPE_INT }, \
	{ "ngroups", CTLTYPE_INT }, \
	{ "job_control", CTLTYPE_INT }, \
	{ "saved_ids", CTLTYPE_INT }, \
	{ "boottime", CTLTYPE_STRUCT }, \
	{ "domainname", CTLTYPE_STRING }, \
	{ "maxpartitions", CTLTYPE_INT }, \
	{ "rawpartition", CTLTYPE_INT }, \
	{ "maxthread", CTLTYPE_INT }, \
	{ "nthreads", CTLTYPE_INT }, \
	{ "osversion", CTLTYPE_STRING }, \
	{ "somaxconn", CTLTYPE_INT }, \
	{ "sominconn", CTLTYPE_INT }, \
	{ "gap", 0 }, \
	{ "gap", 0 }, \
	{ "nosuidcoredump", CTLTYPE_INT }, \
	{ "fsync", CTLTYPE_INT }, \
	{ "sysvmsg", CTLTYPE_INT }, \
	{ "sysvsem", CTLTYPE_INT }, \
	{ "sysvshm", CTLTYPE_INT }, \
	{ "gap", 0 }, \
	{ "msgbufsize", CTLTYPE_INT }, \
	{ "malloc", CTLTYPE_NODE }, \
	{ "cp_time", CTLTYPE_STRUCT }, \
	{ "nchstats", CTLTYPE_STRUCT }, \
	{ "forkstat", CTLTYPE_STRUCT }, \
	{ "gap", 0 }, \
	{ "tty", CTLTYPE_NODE }, \
	{ "ccpu", CTLTYPE_INT }, \
	{ "fscale", CTLTYPE_INT }, \
	{ "nprocs", CTLTYPE_INT }, \
	{ "msgbuf", CTLTYPE_STRUCT }, \
	{ "pool", CTLTYPE_NODE }, \
	{ "stackgap_random", CTLTYPE_INT }, \
	{ "sysvipc_info", CTLTYPE_INT }, \
	{ "allowkmem", CTLTYPE_INT }, \
	{ "witnesswatch", CTLTYPE_INT }, \
	{ "splassert", CTLTYPE_INT }, \
	{ "procargs", CTLTYPE_NODE }, \
	{ "nfiles", CTLTYPE_INT }, \
	{ "ttycount", CTLTYPE_INT }, \
	{ "numvnodes", CTLTYPE_INT }, \
	{ "mbstat", CTLTYPE_STRUCT }, \
	{ "witness", CTLTYPE_NODE }, \
	{ "seminfo", CTLTYPE_STRUCT }, \
	{ "shminfo", CTLTYPE_STRUCT }, \
	{ "intrcnt", CTLTYPE_NODE }, \
	{ "watchdog", CTLTYPE_NODE }, \
	{ "allowdt", CTLTYPE_INT }, \
	{ "proc", CTLTYPE_STRUCT }, \
	{ "maxclusters", CTLTYPE_INT }, \
	{ "evcount", CTLTYPE_NODE }, \
	{ "timecounter", CTLTYPE_NODE }, \
	{ "maxlocksperuid", CTLTYPE_INT }, \
	{ "cp_time2", CTLTYPE_STRUCT }, \
	{ "bufcachepercent", CTLTYPE_INT }, \
	{ "file", CTLTYPE_STRUCT }, \
	{ "wxabort", CTLTYPE_INT }, \
	{ "consdev", CTLTYPE_STRUCT }, \
	{ "netlivelocks", CTLTYPE_INT }, \
	{ "pool_debug", CTLTYPE_INT }, \
	{ "proc_cwd", CTLTYPE_NODE }, \
	{ "proc_nobroadcastkill", CTLTYPE_NODE }, \
	{ "proc_vmmap", CTLTYPE_NODE }, \
	{ "global_ptrace", CTLTYPE_INT }, \
	{ "consbufsize", CTLTYPE_INT }, \
	{ "consbuf", CTLTYPE_STRUCT }, \
	{ "audio", CTLTYPE_STRUCT }, \
	{ "cpustats", CTLTYPE_STRUCT }, \
	{ "pfstatus", CTLTYPE_STRUCT }, \
	{ "timeout_stats", CTLTYPE_STRUCT }, \
	{ "utc_offset", CTLTYPE_INT }, \
	{ "video", CTLTYPE_STRUCT }, \
	{ "clockintr", CTLTYPE_NODE }, \
	{ "autoconf_serial", CTLTYPE_INT }, \
}

/* kern.clockrate */
struct clockinfo {
	int hz;      /* the clock's frequency */
	int tick;    /* microseconds per tick */
	int stathz;  /* the statistics clock's frequency */
	int profhz;  /* the profiling clock's frequency */
};

/* ---- vm ---- */
#define VM_METER       1  /* struct */
#define VM_LOADAVG     2  /* struct loadavg */
#define VM_PSSTRINGS   3  /* struct */
#define VM_UVMEXP      4  /* struct */
#define VM_SWAPENCRYPT 5  /* node */
#define VM_NKMEMPAGES  6  /* int */
#define VM_ANONMIN     7  /* int */
#define VM_VTEXTMIN    8  /* int */
#define VM_VNODEMIN    9  /* int */
#define VM_MAXSLP      10 /* int */
#define VM_USPACE      11 /* int */
#define VM_MALLOC_CONF 12 /* string */
#define VM_MAXID       13

#define CTL_VM_NAMES { \
	{ 0, 0 }, \
	{ "vmmeter", CTLTYPE_STRUCT }, \
	{ "loadavg", CTLTYPE_STRUCT }, \
	{ "psstrings", CTLTYPE_STRUCT }, \
	{ "uvmexp", CTLTYPE_STRUCT }, \
	{ "swapencrypt", CTLTYPE_NODE }, \
	{ "nkmempages", CTLTYPE_INT }, \
	{ "anonmin", CTLTYPE_INT }, \
	{ "vtextmin", CTLTYPE_INT }, \
	{ "vnodemin", CTLTYPE_INT }, \
	{ "maxslp", CTLTYPE_INT }, \
	{ "uspace", CTLTYPE_INT }, \
	{ "malloc_conf", CTLTYPE_STRING }, \
}

/* vm.loadavg: the number of runnable processes, averaged over 1, 5 and 15
   minutes, as fixed-point numbers; divide by fscale */
struct loadavg {
	unsigned int ldavg[3];
	long fscale;
};
#define FSHIFT 11 /* bits to the right of the point */
#define FSCALE (1 << FSHIFT)

/* ---- hw ---- */
#define HW_MACHINE        1  /* string: the machine class */
#define HW_MODEL          2  /* string: the processor's model */
#define HW_NCPU           3  /* int: processors configured */
#define HW_BYTEORDER      4  /* int: 1234 */
#define HW_PHYSMEM        5  /* int (named only as a gap: hw.physmem is HW_PHYSMEM64) */
#define HW_USERMEM        6  /* int (a gap in the names, like HW_PHYSMEM) */
#define HW_PAGESIZE       7  /* int: bytes of a page */
#define HW_DISKNAMES      8  /* string: the disks, comma-separated */
#define HW_DISKSTATS      9  /* struct */
#define HW_DISKCOUNT      10 /* int: number of disks */
#define HW_SENSORS        11 /* node */
#define HW_CPUSPEED       12 /* int: MHz */
#define HW_SETPERF        13 /* int */
#define HW_VENDOR         14 /* string */
#define HW_PRODUCT        15 /* string */
#define HW_VERSION        16 /* string */
#define HW_SERIALNO       17 /* string */
#define HW_UUID           18 /* string */
#define HW_PHYSMEM64      19 /* quad: bytes of memory, named hw.physmem */
#define HW_USERMEM64      20 /* quad, named hw.usermem */
#define HW_NCPUFOUND      21 /* int: processors found */
#define HW_ALLOWPOWERDOWN 22 /* int */
#define HW_PERFPOLICY     23 /* string */
#define HW_SMT            24 /* int */
#define HW_NCPUONLINE     25 /* int: processors running */
#define HW_POWER          26 /* int */
#define HW_BATTERY        27 /* node */
#define HW_UCOMNAMES      28 /* string */
#define HW_BLOCKCPU       29 /* string */
#define HW_MAXID          30

#define CTL_HW_NAMES { \
	{ 0, 0 }, \
	{ "machine", CTLTYPE_STRING }, \
	{ "model", CTLTYPE_STRING }, \
	{ "ncpu", CTLTYPE_INT }, \
	{ "byteorder", CTLTYPE_INT }, \
	{ "gap", 0 }, \
	{ "gap", 0 }, \
	{ "pagesize", CTLTYPE_INT }, \
	{ "disknames", CTLTYPE_STRING }, \
	{ "diskstats", CTLTYPE_STRUCT }, \
	{ "diskcount", CTLTYPE_INT }, \
	{ "sensors", CTLTYPE_NODE }, \
	{ "cpuspeed", CTLTYPE_INT }, \
	{ "setperf", CTLTYPE_INT }, \
	{ "vendor", CTLTYPE_STRING }, \
	{ "product", CTLTYPE_STRING }, \
	{ "version", CTLTYPE_STRING }, \
	{ "serialno", CTLTYPE_STRING }, \
	{ "uuid", CTLTYPE_STRING }, \
	{ "physmem", CTLTYPE_QUAD }, \
	{ "usermem", CTLTYPE_QUAD }, \
	{ "ncpufound", CTLTYPE_INT }, \
	{ "allowpowerdown", CTLTYPE_INT }, \
	{ "perfpolicy", CTLTYPE_STRING }, \
	{ "smt", CTLTYPE_INT }, \
	{ "ncpuonline", CTLTYPE_INT }, \
	{ "power", CTLTYPE_INT }, \
	{ "battery", CTLTYPE_NODE }, \
	{ "ucomnames", CTLTYPE_STRING }, \
	{ "blockcpu", CTLTYPE_STRING }, \
}

/* ---- net: family, protocol, variable ----
   The families are numbered as <sys/socket.h> does (AF_UNIX 1, AF_INET 2,
   AF_INET6 10), the protocols as IPPROTO_*. */
#define NET_MAXID 11

#define CTL_NET_NAMES { \
	{ 0, 0 }, \
	{ "unix", CTLTYPE_NODE }, \
	{ "inet", CTLTYPE_NODE }, \
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, \
	{ "inet6", CTLTYPE_NODE }, \
}

/* net.inet.<protocol> */
#define CTL_IPPROTO_NAMES { \
	{ "ip", CTLTYPE_NODE }, \
	{ "icmp", CTLTYPE_NODE }, \
	{ "igmp", CTLTYPE_NODE }, \
	{ "ggp", CTLTYPE_NODE }, \
	{ "ipip", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ "tcp", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ "egp", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "pup", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
}

/* net.inet.ip.<variable> */
#define IPCTL_FORWARDING            1  /* int: act as a router */
#define IPCTL_SENDREDIRECTS         2  /* int */
#define IPCTL_DEFTTL                3  /* int: the default TTL */
#define IPCTL_SOURCEROUTE           5  /* int */
#define IPCTL_DIRECTEDBCAST         6  /* int */
#define IPCTL_IPPORT_FIRSTAUTO      7  /* int */
#define IPCTL_IPPORT_LASTAUTO       8  /* int */
#define IPCTL_IPPORT_HIFIRSTAUTO    9  /* int */
#define IPCTL_IPPORT_HILASTAUTO     10 /* int */
#define IPCTL_IPPORT_MAXQUEUE       11 /* int */
#define IPCTL_ENCDEBUG              12 /* int */
#define IPCTL_IPSEC_STATS           13
#define IPCTL_IPSEC_EXPIRE_ACQUIRE  14 /* int */
#define IPCTL_IPSEC_EMBRYONIC_SA_TIMEOUT 15 /* int */
#define IPCTL_IPSEC_REQUIRE_PFS     16 /* int */
#define IPCTL_IPSEC_SOFT_ALLOCATIONS 17 /* int */
#define IPCTL_IPSEC_ALLOCATIONS     18 /* int */
#define IPCTL_IPSEC_SOFT_BYTES      19 /* int */
#define IPCTL_IPSEC_BYTES           20 /* int */
#define IPCTL_IPSEC_TIMEOUT         21 /* int */
#define IPCTL_IPSEC_SOFT_TIMEOUT    22 /* int */
#define IPCTL_IPSEC_SOFT_FIRSTUSE   23 /* int */
#define IPCTL_IPSEC_FIRSTUSE        24 /* int */
#define IPCTL_IPSEC_ENC_ALGORITHM   25 /* string */
#define IPCTL_IPSEC_AUTH_ALGORITHM  26 /* string */
#define IPCTL_MTUDISC               27 /* int */
#define IPCTL_MTUDISCTIMEOUT        28 /* int */
#define IPCTL_IPSEC_IPCOMP_ALGORITHM 29 /* string */
#define IPCTL_IFQUEUE               30 /* node */
#define IPCTL_MFORWARDING           31 /* int */
#define IPCTL_MULTIPATH             32 /* int */
#define IPCTL_STATS                 33 /* struct */
#define IPCTL_MRTPROTO              34 /* int */
#define IPCTL_MRTSTATS              35 /* struct */
#define IPCTL_ARPQUEUED             36 /* int */
#define IPCTL_MRTMFC                37 /* struct */
#define IPCTL_MRTVIF                38 /* struct */
#define IPCTL_ARPTIMEOUT            39 /* int */
#define IPCTL_ARPDOWN               40 /* int */
#define IPCTL_ARPQUEUE              41 /* node */
#define IPCTL_MAXID                 42

#define IPCTL_NAMES { \
	{ 0, 0 }, \
	{ "forwarding", CTLTYPE_INT }, \
	{ "redirect", CTLTYPE_INT }, \
	{ "ttl", CTLTYPE_INT }, \
	{ 0, 0 }, \
	{ "sourceroute", CTLTYPE_INT }, \
	{ "directed-broadcast", CTLTYPE_INT }, \
	{ "portfirst", CTLTYPE_INT }, \
	{ "portlast", CTLTYPE_INT }, \
	{ "porthifirst", CTLTYPE_INT }, \
	{ "porthilast", CTLTYPE_INT }, \
	{ "maxqueue", CTLTYPE_INT }, \
	{ "encdebug", CTLTYPE_INT }, \
	{ 0, 0 }, \
	{ "ipsec-expire-acquire", CTLTYPE_INT }, \
	{ "ipsec-invalid-life", CTLTYPE_INT }, \
	{ "ipsec-pfs", CTLTYPE_INT }, \
	{ "ipsec-soft-allocs", CTLTYPE_INT }, \
	{ "ipsec-allocs", CTLTYPE_INT }, \
	{ "ipsec-soft-bytes", CTLTYPE_INT }, \
	{ "ipsec-bytes", CTLTYPE_INT }, \
	{ "ipsec-timeout", CTLTYPE_INT }, \
	{ "ipsec-soft-timeout", CTLTYPE_INT }, \
	{ "ipsec-soft-firstuse", CTLTYPE_INT }, \
	{ "ipsec-firstuse", CTLTYPE_INT }, \
	{ "ipsec-enc-alg", CTLTYPE_STRING }, \
	{ "ipsec-auth-alg", CTLTYPE_STRING }, \
	{ "mtudisc", CTLTYPE_INT }, \
	{ "mtudisctimeout", CTLTYPE_INT }, \
	{ "ipsec-comp-alg", CTLTYPE_STRING }, \
	{ "ifq", CTLTYPE_NODE }, \
	{ "mforwarding", CTLTYPE_INT }, \
	{ "multipath", CTLTYPE_INT }, \
	{ "stats", CTLTYPE_STRUCT }, \
	{ "mrtproto", CTLTYPE_INT }, \
	{ "mrtstats", CTLTYPE_STRUCT }, \
	{ "arpqueued", CTLTYPE_INT }, \
	{ "mrtmfc", CTLTYPE_STRUCT }, \
	{ "mrtvif", CTLTYPE_STRUCT }, \
	{ "arptimeout", CTLTYPE_INT }, \
	{ "arpdown", CTLTYPE_INT }, \
	{ "arpq", CTLTYPE_NODE }, \
}

/* ---- debug, machdep, ddb, vfs: nothing below them yet ---- */
#define CTL_DEBUG_NAME  0 /* string: the variable's name */
#define CTL_DEBUG_VALUE 1 /* int: its value */
#define CTL_DEBUG_MAXID 20

#ifdef __cplusplus
extern "C" {
#endif
/* Read the value at the path into oldp (*oldlenp is the room, then the size;
   oldp NULL asks for the size only) and/or write newp (newlen bytes; root).
   Native ABI only: a Linux binary gets ENOSYS. */
int sysctl(const int *name, unsigned int namelen, void *oldp, size_t *oldlenp, void *newp, size_t newlen);
#ifdef __cplusplus
}
#endif

#endif
