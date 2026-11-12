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

/* kern_sysctl.c - sysctl(2): the system's variables, named by a path of
   integers (sys/sysctl.h, OpenBSD's numbering).

   sys_sysctl takes the path, hands what is below the first number to the
   function of that subtree, and copies the size back.  Reading follows
   OpenBSD's rules: a NULL old buffer asks for the size only, a buffer that
   is too small fails with ENOMEM, a write to a value that is read-only
   fails with EPERM, and an id that is reserved but not answered here fails
   with EOPNOTSUPP.  Only root writes.  The values that can be written are
   the host name and the domain name.

   Every value comes from the place that owns it (version strings, the
   uname names in kern_xxx.c, the page count, the disks of subr_disk.c);
   the load average is the one thing kept here: a kernel thread samples
   the number of runnable tasks every five seconds. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/conf.h>
#include <sys/socket.h>
#include <net/if_var.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>

extern char uts_nodename[65], uts_domain[65];
extern struct cfdriver disk_cd;

char cpu_model[64] = "unknown processor"; /* set by cpu_attach */

/* ---- the answers of one node ---- */

/* a read-only value of n bytes */
static long rd_bytes(void *oldp, size_t *oldlenp, void *newp, const void *v, size_t n) {
  if (oldp && *oldlenp < n)
    return -ENOMEM;
  if (newp)
    return -EPERM;
  *oldlenp = n;
  if (oldp && copy_to_user(oldp, v, n))
    return -EFAULT;
  return 0;
}

static long rd_int(void *oldp, size_t *oldlenp, void *newp, int v) { return rd_bytes(oldp, oldlenp, newp, &v, sizeof v); }
static long rd_quad(void *oldp, size_t *oldlenp, void *newp, int64_t v) {
  return rd_bytes(oldp, oldlenp, newp, &v, sizeof v);
}
static long rd_string(void *oldp, size_t *oldlenp, void *newp, const char *s) {
  return rd_bytes(oldp, oldlenp, newp, s, strlen(s) + 1);
}

/* a string that can be set (the new one is newlen bytes without its NUL;
   maxlen counts the NUL).  A buffer that is too small gets the string cut
   short, as long as there is room for the NUL (an empty one is ENOMEM). */
static long rw_string(void *oldp, size_t *oldlenp, void *newp, size_t newlen, char *str, size_t maxlen) {
  size_t len = strlen(str) + 1;
  char tmp[128];
  if (oldp && *oldlenp == 0)
    return -ENOMEM;
  if (newp && (newlen >= maxlen || newlen >= sizeof tmp))
    return -EINVAL;
  if (newp && newlen && copy_from_user(tmp, newp, newlen))
    return -EFAULT;
  if (oldp) {
    if (*oldlenp < len) {
      len = *oldlenp;
      if (copy_to_user(oldp, str, len - 1) || copy_to_user((char *)oldp + len - 1, "", 1))
        return -EFAULT;
    } else if (copy_to_user(oldp, str, len)) {
      return -EFAULT;
    }
  }
  *oldlenp = len;
  if (newp) {
    memcpy(str, tmp, newlen);
    str[newlen] = 0;
  }
  return 0;
}

/* ---- the load average ---- */

static struct loadavg averunnable = {{0, 0, 0}, FSCALE};

/* exp(-5/60), exp(-5/300) and exp(-5/900) as fixed-point numbers: the
   decay of the three averages over one 5 second sample */
static const unsigned cexp[3] = {1884, 2014, 2037};

static long loadavg_thread(void *arg) {
  (void)arg;
  for (;;) {
    sleep_ns(5ULL * 1000000000ULL);
    unsigned n = 0;
    uint64_t f = irq_save();
    list_for_each(p, &all_tasks) {
      struct task *t = list_entry(p, struct task, all_link);
      /* what is running or waits for a disk, not the kernel's own threads
         (this one is running now, the idle task always is) */
      if (!t->is_kthread && (t->state == T_RUNNING || t->state == T_UNINTERRUPTIBLE))
        n++;
    }
    irq_restore(f);
    for (int i = 0; i < 3; i++) {
      uint64_t a = averunnable.ldavg[i];
      a = (a * cexp[i] + (uint64_t)n * FSCALE * (FSCALE - cexp[i])) >> FSHIFT;
      averunnable.ldavg[i] = (unsigned)a;
    }
  }
  return 0;
}

void loadavg_init(void) { kthread_create("loadavg", loadavg_thread, NULL); }

/* ---- kern ---- */

static long kern_sysctl(int *name, unsigned namelen, void *oldp, size_t *oldlenp, void *newp, size_t newlen) {
  if (namelen != 1)
    return -ENOTDIR; /* every node answered here is a leaf */
  switch (name[0]) {
  case KERN_OSTYPE:
    return rd_string(oldp, oldlenp, newp, ostype);
  case KERN_OSRELEASE:
    return rd_string(oldp, oldlenp, newp, osrelease);
  case KERN_OSVERSION:
    return rd_string(oldp, oldlenp, newp, osversion);
  case KERN_VERSION:
    return rd_string(oldp, oldlenp, newp, version);
  case KERN_HOSTNAME:
    return rw_string(oldp, oldlenp, newp, newlen, uts_nodename, sizeof uts_nodename);
  case KERN_DOMAINNAME:
    return rw_string(oldp, oldlenp, newp, newlen, uts_domain, sizeof uts_domain);
  case KERN_BOOTTIME: {
    /* when the clock says the kernel started: it moves with the clock */
    int64_t ns = (int64_t)realtime_ns() - (int64_t)monotonic_ns();
    int64_t bt[2] = {ns / 1000000000, (ns % 1000000000) / 1000}; /* struct timeval */
    return rd_bytes(oldp, oldlenp, newp, bt, sizeof bt);
  }
  case KERN_CLOCKRATE: {
    struct clockinfo ci = {HZ, 1000000 / HZ, HZ, HZ};
    return rd_bytes(oldp, oldlenp, newp, &ci, sizeof ci);
  }
  case KERN_MAXPROC:
    return rd_int(oldp, oldlenp, newp, maxproc);
  case KERN_MAXFILES:
    return rd_int(oldp, oldlenp, newp, maxfiles);
  case KERN_ARGMAX:
    return rd_int(oldp, oldlenp, newp, argmax);
  case KERN_POSIX1:
    return rd_int(oldp, oldlenp, newp, 202405);
  case KERN_NGROUPS:
    return rd_int(oldp, oldlenp, newp, NGROUPS_MAX);
  case KERN_JOB_CONTROL:
  case KERN_SAVED_IDS:
  case KERN_SYSVMSG:
  case KERN_SYSVSEM:
  case KERN_SYSVSHM:
  case KERN_FSYNC:
    return rd_int(oldp, oldlenp, newp, 1);
  case KERN_FSCALE:
    return rd_int(oldp, oldlenp, newp, FSCALE);
  case KERN_NPROCS: {
    int n = 0;
    uint64_t f = irq_save();
    list_for_each(p, &all_procs) n++;
    irq_restore(f);
    return rd_int(oldp, oldlenp, newp, n);
  }
  case KERN_MSGBUFSIZE:
    return rd_int(oldp, oldlenp, newp, MSGBUFSIZE);
  case KERN_MSGBUF: {
    /* the text of the kernel's log buffer (dmesg(8)), not OpenBSD's struct msgbuf */
    if (newp)
      return -EPERM;
    char *tmp = kmalloc(MSGBUFSIZE);
    if (!tmp)
      return -ENOMEM;
    long r = rd_bytes(oldp, oldlenp, newp, tmp, msgbuf_read(tmp, MSGBUFSIZE));
    kfree(tmp);
    return r;
  }
  case KERN_NTHREADS: {
    int n = 0;
    uint64_t f = irq_save();
    list_for_each(p, &all_tasks) {
      if (!list_entry(p, struct task, all_link)->is_kthread)
        n++;
    }
    irq_restore(f);
    return rd_int(oldp, oldlenp, newp, n);
  }
  default:
    return -EOPNOTSUPP;
  }
}

/* ---- hw ---- */

/* "disk0:,disk1:": OpenBSD's list (a name, a colon and the disk's unique id,
   which these disks do not have) */
static void disknames(char *buf, size_t size) {
  size_t n = 0;
  buf[0] = 0;
  for (int i = 0; i < disk_cd.cd_ndevs; i++) {
    struct device *d = disk_cd.cd_devs[i];
    if (!d)
      continue;
    int w = snprintf(buf + n, size - n, "%s%s:", n ? "," : "", d->dv_xname);
    if (w < 0 || (size_t)w >= size - n)
      break;
    n += (size_t)w;
  }
}

static int ndisks(void) {
  int n = 0;
  for (int i = 0; i < disk_cd.cd_ndevs; i++)
    if (disk_cd.cd_devs[i])
      n++;
  return n;
}

static long hw_sysctl(int *name, unsigned namelen, void *oldp, size_t *oldlenp, void *newp, size_t newlen) {
  (void)newlen;
  if (namelen != 1)
    return -ENOTDIR;
  switch (name[0]) {
  case HW_MACHINE:
    return rd_string(oldp, oldlenp, newp, "amd64");
  case HW_MODEL:
    return rd_string(oldp, oldlenp, newp, cpu_model);
  case HW_NCPU:
  case HW_NCPUFOUND:
  case HW_NCPUONLINE:
    return rd_int(oldp, oldlenp, newp, 1);
  case HW_BYTEORDER:
    return rd_int(oldp, oldlenp, newp, 1234);
  case HW_PAGESIZE:
    return rd_int(oldp, oldlenp, newp, PAGE_SIZE);
  case HW_PHYSMEM64:
    return rd_quad(oldp, oldlenp, newp, (int64_t)(total_pages * PAGE_SIZE));
  case HW_CPUSPEED:
    return rd_int(oldp, oldlenp, newp, (int)(tsc_frequency() / 1000000));
  case HW_DISKCOUNT:
    return rd_int(oldp, oldlenp, newp, ndisks());
  case HW_DISKNAMES: {
    char buf[128];
    disknames(buf, sizeof buf);
    return rd_string(oldp, oldlenp, newp, buf);
  }
  default:
    return -EOPNOTSUPP;
  }
}

/* ---- vm ---- */

static long vm_sysctl(int *name, unsigned namelen, void *oldp, size_t *oldlenp, void *newp, size_t newlen) {
  (void)newlen;
  if (namelen != 1)
    return -ENOTDIR;
  switch (name[0]) {
  case VM_LOADAVG: {
    struct loadavg la = averunnable;
    return rd_bytes(oldp, oldlenp, newp, &la, sizeof la);
  }
  default:
    return -EOPNOTSUPP;
  }
}

/* an int that can be set, within min..max */
static long rw_int(void *oldp, size_t *oldlenp, void *newp, size_t newlen, int *var, int min, int max) {
  int v;
  if (oldp && *oldlenp < sizeof v)
    return -ENOMEM;
  if (newp && newlen != sizeof v)
    return -EINVAL;
  if (newp && copy_from_user(&v, newp, sizeof v))
    return -EFAULT;
  if (newp && (v < min || v > max))
    return -EINVAL;
  *oldlenp = sizeof *var;
  if (oldp && copy_to_user(oldp, var, sizeof *var))
    return -EFAULT;
  if (newp)
    *var = v;
  return 0;
}

/* net.route: the routing table, as BSD's route(8) and netstat(1) ask for it
   (net, AF_ROUTE, 0, AF_INET or 0, NET_RT_DUMP, 0), but an array of struct
   rtinfo.  net.inet.icmp.bmcastecho, net.inet.icmp.errppslimit and
   net.inet.tcp.rstppslimit are the variables of the kernel's IP; the rest of
   net.* answers nothing yet. */
static long net_sysctl(int *name, unsigned namelen, void *oldp, size_t *oldlenp, void *newp, size_t newlen) {
  if (namelen == 3 && name[0] == AF_INET) {
    if (name[1] == IPPROTO_ICMP && name[2] == ICMPCTL_BMCASTECHO)
      return rw_int(oldp, oldlenp, newp, newlen, &icmp_bmcastecho, 0, 1);
    if (name[1] == IPPROTO_ICMP && name[2] == ICMPCTL_ERRPPSLIMIT)
      return rw_int(oldp, oldlenp, newp, newlen, &icmp_errppslimit, -1, 1000000);
    if (name[1] == IPPROTO_TCP && name[2] == TCPCTL_RSTPPSLIMIT)
      return rw_int(oldp, oldlenp, newp, newlen, &tcp_rstppslimit, -1, 1000000);
    return -EOPNOTSUPP;
  }
  if (namelen != 5 || name[0] != AF_ROUTE || name[1] != 0 || (name[2] != 0 && name[2] != AF_INET) ||
      name[3] != NET_RT_DUMP)
    return -EOPNOTSUPP;
  char tmp[RTMAX * sizeof(struct rtinfo)];
  return rd_bytes(oldp, oldlenp, newp, tmp, (size_t)rt_dump(tmp, sizeof tmp));
}

/* sysctl(name, namelen, oldp, oldlenp, newp, newlen).  machdep, debug, ddb
   and vfs are reserved. */
long sys_sysctl(long uname, long namelen, long oldp, long oldlenp, long newp, long newlen) {
  int name[CTL_MAXNAME];
  size_t oldlen = 0;
  long err;
  if (newp && !capable())
    return -EPERM;
  if (namelen < 2 || namelen > CTL_MAXNAME)
    return -EINVAL; /* every top-level name is a node */
  if (copy_from_user(name, (void *)uname, (size_t)namelen * sizeof(int)))
    return -EFAULT;
  if (oldlenp && copy_from_user(&oldlen, (void *)oldlenp, sizeof oldlen))
    return -EFAULT;
  if (oldp && !oldlenp)
    return -EINVAL; /* a buffer without its size */
  switch (name[0]) {
  case CTL_KERN:
    err = kern_sysctl(name + 1, (unsigned)namelen - 1, (void *)oldp, &oldlen, (void *)newp, (size_t)newlen);
    break;
  case CTL_HW:
    err = hw_sysctl(name + 1, (unsigned)namelen - 1, (void *)oldp, &oldlen, (void *)newp, (size_t)newlen);
    break;
  case CTL_VM:
    err = vm_sysctl(name + 1, (unsigned)namelen - 1, (void *)oldp, &oldlen, (void *)newp, (size_t)newlen);
    break;
  case CTL_NET:
    err = net_sysctl(name + 1, (unsigned)namelen - 1, (void *)oldp, &oldlen, (void *)newp, (size_t)newlen);
    break;
  default:
    return -EOPNOTSUPP;
  }
  if (err)
    return err;
  if (oldlenp && copy_to_user((void *)oldlenp, &oldlen, sizeof oldlen))
    return -EFAULT;
  return 0;
}
