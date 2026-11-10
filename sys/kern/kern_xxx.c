/* misc.c - uname, host names, sysinfo, reboot, syslog, capabilities. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>

/* uname: the system says it is Linux, so that programs that check (glibc's
   "kernel too old" test, configure scripts) treat it as one */
#ifndef UTS_SYSNAME
#define UTS_SYSNAME "Linux"
#endif
const char *uts_sysname = UTS_SYSNAME;
const char *uts_release = "6.9.0-shall";
const char *uts_version = "#1 Shall 0.9.1";
char uts_nodename[65] = "shall";
static char uts_domain[65] = "(none)";

long sys_uname(long ubuf) {
  char u[6][65];
  memset(u, 0, sizeof(u));
  strcpy(u[0], uts_sysname);
  strcpy(u[1], uts_nodename);
  strcpy(u[2], uts_release);
  strcpy(u[3], uts_version);
  strcpy(u[4], "x86_64");
  strcpy(u[5], uts_domain);
  return copy_to_user((void *)ubuf, u, sizeof(u)) ? -EFAULT : 0;
}

static long set_name(char *dst, long uname, long len) {
  if (!capable())
    return -EPERM;
  if (len < 0 || len > 64)
    return -EINVAL;
  char tmp[65];
  if (copy_from_user(tmp, (void *)uname, (size_t)len))
    return -EFAULT;
  tmp[len] = 0;
  strcpy(dst, tmp);
  return 0;
}

long sys_sethostname(long name, long len) { return set_name(uts_nodename, name, len); }
long sys_setdomainname(long name, long len) { return set_name(uts_domain, name, len); }

struct ksysinfo {
  int64_t uptime;
  uint64_t loads[3], totalram, freeram, sharedram, bufferram, totalswap, freeswap;
  uint16_t procs, pad;
  uint32_t pad2;
  uint64_t totalhigh, freehigh;
  uint32_t mem_unit;
  char rest[4];
};

long sys_sysinfo(long u) {
  struct ksysinfo si;
  memset(&si, 0, sizeof(si));
  si.uptime = (int64_t)(monotonic_ns() / 1000000000UL);
  si.totalram = total_pages * PAGE_SIZE;
  si.freeram = free_pages_count * PAGE_SIZE;
  int n = 0;
  list_for_each(p, &all_tasks) n++;
  si.procs = (uint16_t)n;
  si.mem_unit = 1;
  return copy_to_user((void *)u, &si, sizeof(si)) ? -EFAULT : 0;
}

#define REBOOT_MAGIC1 0xfee1dead
#define RB_HALT 0xcdef0123
#define RB_POWER_OFF 0x4321fedc
#define RB_RESTART 0x01234567
#define RB_CAD_ON 0x89abcdef
#define RB_CAD_OFF 0

long sys_reboot(long m1, long m2, long cmd) {
  (void)m2;
  if (!capable())
    return -EPERM;
  if ((uint32_t)m1 != REBOOT_MAGIC1)
    return -EINVAL;
  if ((uint32_t)cmd == RB_CAD_ON || cmd == RB_CAD_OFF)
    return 0;
  sync_all();
  bsync(NULL);
  list_for_each(p, &mounts) {
    struct mount *m = list_entry(p, struct mount, link);
    if (m->sb->ops && m->sb->ops->shutdown)
      m->sb->ops->shutdown(m->sb);
  }
  printk("System %s.\n", (uint32_t)cmd == RB_RESTART ? "restarting" : "halted");
  cli();
  if ((uint32_t)cmd == RB_RESTART) {
    outb(0x64, 0xfe); /* keyboard controller reset */
  } else if ((uint32_t)cmd == RB_POWER_OFF) {
    outw(0x604, 0x2000);  /* QEMU q35 / piix ACPI */
    outw(0xb004, 0x2000); /* older QEMU, Bochs */
  }
  for (;;)
    hlt();
}

/* syslog(2): the kernel log */
long sys_syslog(long type, long ubuf, long len) {
  switch (type) {
  case 0:
  case 1:
  case 5:
  case 6:
  case 7:
  case 8:
    return 0;
  case 2:
  case 3:
  case 4: {
    if (len < 0)
      return -EINVAL;
    char *tmp = kmalloc(65536);
    if (!tmp)
      return -ENOMEM;
    size_t n = log_read(tmp, MIN((size_t)len, 65536UL));
    long r = copy_to_user((void *)ubuf, tmp, n) ? -EFAULT : (long)n;
    kfree(tmp);
    return r;
  }
  case 9:
    return 0;
  case 10:
    return 65536;
  }
  return -EINVAL;
}

/* capabilities: root has them all, others none */
long sys_capget(long uhdr, long udata) {
  uint32_t hdr[2];
  if (copy_from_user(hdr, (void *)uhdr, 8))
    return -EFAULT;
  if (!udata)
    return 0;
  uint32_t d[6] = {0};
  if (capable())
    for (int i = 0; i < 6; i++)
      d[i] = i % 3 == 2 ? 0 : 0xffffffff;
  return copy_to_user((void *)udata, d, sizeof(d)) ? -EFAULT : 0;
}

long sys_capset(long uhdr, long udata) {
  (void)uhdr;
  (void)udata;
  return capable() ? 0 : -EPERM;
}
