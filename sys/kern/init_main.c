/* main.c - kmain: from the multiboot hand-over to the first process. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <uvm/uvm.h>
#include <net/if_var.h>
#include <machine/cpu.h>

extern char _kernel_end[];
char kernel_cmdline[512];
static uint64_t ranges[64], reserved[32];
static int nranges, nreserved;

void pit_init(void (*tick)(void));
long flush_thread(void *arg);
NORETURN void cpu_idle(void);
void enter_user(struct regs *r);

static uint32_t rd32(uint64_t phys) { return *(uint32_t *)P2V(phys); }

static void parse_multiboot(uint32_t magic, uint64_t mbi) {
  if (magic != 0x2BADB002)
    panic("not started by a multiboot loader (magic %#x)", magic);
  uint32_t flags = rd32(mbi);
  if (flags & 4) {
    const char *c = P2V((uint64_t)rd32(mbi + 16));
    size_t n = strnlen(c, sizeof(kernel_cmdline) - 1);
    memcpy(kernel_cmdline, c, n);
    kernel_cmdline[n] = 0;
  }
  reserved[nreserved++] = PAGE_DOWN(mbi);
  reserved[nreserved++] = PAGE_UP(mbi + 120);
  if (flags & 8) {
    uint32_t count = rd32(mbi + 20), addr = rd32(mbi + 24);
    for (uint32_t i = 0; i < count && nreserved < 30; i++) {
      reserved[nreserved++] = PAGE_DOWN(rd32(addr + 16 * i));
      reserved[nreserved++] = PAGE_UP(rd32(addr + 16 * i + 4));
    }
  }
  if (flags & 64) {
    uint32_t len = rd32(mbi + 44), addr = rd32(mbi + 48);
    for (uint32_t off = 0; off < len && nranges < 32;) {
      uint32_t size = rd32(addr + off);
      uint64_t base = *(uint64_t *)P2V(addr + off + 4);
      uint64_t l = *(uint64_t *)P2V(addr + off + 12);
      uint32_t type = rd32(addr + off + 20);
      if (type == 1) {
        ranges[2 * nranges] = base;
        ranges[2 * nranges + 1] = base + l;
        nranges++;
      }
      off += size + 4;
    }
  } else if (flags & 1) {
    ranges[0] = 0x100000;
    ranges[1] = 0x100000 + (uint64_t)rd32(mbi + 8) * 1024;
    nranges = 1;
  }
  if (!nranges)
    panic("no memory map");
}

/* "name=value" from the command line */
static bool cmdline_get(const char *name, char *out, size_t n) {
  size_t nl = strlen(name);
  const char *s = kernel_cmdline;
  while (*s) {
    while (*s == ' ')
      s++;
    if (!strncmp(s, name, nl) && s[nl] == '=') {
      s += nl + 1;
      size_t k = 0;
      while (*s && *s != ' ' && k + 1 < n)
        out[k++] = *s++;
      out[k] = 0;
      return true;
    }
    while (*s && *s != ' ')
      s++;
  }
  return false;
}

static bool cmdline_flag(const char *name) {
  size_t nl = strlen(name);
  const char *s = kernel_cmdline;
  while (*s) {
    while (*s == ' ')
      s++;
    if (!strncmp(s, name, nl) && (s[nl] == ' ' || !s[nl]))
      return true;
    while (*s && *s != ' ')
      s++;
  }
  return false;
}

static void mount_root_fs(void) {
  char root[64] = "", type[32] = "ext2";
  cmdline_get("root", root, sizeof(root));
  if (!root[0]) /* without root=: the first disk, ATA or virtio */
    snprintf(root, sizeof(root), "/dev/%s", blockdev_nth(0) ? blockdev_nth(0)->name : "sda");
  cmdline_get("rootfstype", type, sizeof(type));
  const char *name = !strncmp(root, "/dev/", 5) ? root + 5 : root;
  struct blockdev *b = blockdev_by_name(name);
  int e = -ENODEV;
  if (b) {
    e = mount_root(type, b);
    if (!e && cmdline_flag("ro"))
      root_mount->sb->rdonly = true, root_mount->flags |= MS_RDONLY;
  }
  if (e) {
    printk("cannot mount the root file system %s (%s): error %d; using an empty tmpfs\n", root, type, -e);
    if (mount_root("tmpfs", NULL))
      panic("no root file system");
    return;
  }
  printk("root file system: %s (%s%s)\n", root, type, root_mount->sb->rdonly ? ", read-only" : "");
}

static void auto_mount(const char *dir, const char *type) {
  int e = do_mount(type, dir, type, 0, NULL);
  if (e && e != -ENOENT)
    printk("cannot mount %s on %s: error %d\n", type, dir, -e);
}

static int open_console(void) {
  int err;
  struct file *f = open_path(AT_FDCWD, "/dev/console", O_RDWR, 0, &err);
  if (!f)
    return err;
  /* fds 0, 1, 2 */
  fd_install(f, 0, 0);
  file_get(f);
  fd_install(f, 1, 0);
  file_get(f);
  fd_install(f, 2, 0);
  return 0;
}

static long run_init(void *arg) {
  (void)arg;
  /* V86BSD has no mount utility (mount is not POSIX): the kernel mounts
     /dev and /proc itself, as Linux's CONFIG_DEVTMPFS_MOUNT does for /dev */
  auto_mount("/dev", "devtmpfs");
  auto_mount("/proc", "proc");
  if (open_console())
    printk("warning: unable to open an initial console\n");
  /* as Linux: name=value words the kernel does not know go to init's
     environment */
  static const char *known[] = {"root=", "rootfstype=", "init=", "console=", NULL};
  static char envbuf[512];
  const char *envp[32] = {"HOME=/", "TERM=linux"};
  int ne = 2;
  memcpy(envbuf, kernel_cmdline, sizeof(envbuf));
  for (char *w = envbuf; *w && ne < 31;) {
    while (*w == ' ')
      *w++ = 0;
    char *start = w;
    while (*w && *w != ' ')
      w++;
    if (*w)
      *w++ = 0;
    if (!strchr(start, '='))
      continue;
    bool k = false;
    for (int i = 0; known[i]; i++)
      if (!strncmp(start, known[i], strlen(known[i])))
        k = true;
    if (!k)
      envp[ne++] = start;
  }
  envp[ne] = NULL;
  char init[128];
  const char *tries[] = {NULL, "/sbin/init", "/etc/init", "/bin/init", "/bin/sh"};
  if (cmdline_get("init", init, sizeof(init)))
    tries[0] = init;
  for (size_t i = 0; i < ARRAY_SIZE(tries); i++) {
    if (!tries[i])
      continue;
    const char *argv[] = {tries[i], NULL};
    int e = kernel_execve(tries[i], argv, (const char *const *)envp);
    if (e == 0) {
      cli();
      enter_user(current->regs);
    }
    if (e != -ENOENT)
      printk("cannot run %s: error %d\n", tries[i], -e);
  }
  panic("no init found: try passing init= to the kernel");
}

void kmain(uint32_t magic, uint32_t mbi) {
  serial_init();
  vga_init();
  printk("%s", version);
  parse_multiboot(magic, mbi);
  printk("command line: %s\n", kernel_cmdline);
  page_init((uint64_t)_kernel_end - KERNEL_BASE, ranges, nranges, reserved, nreserved);
  vm_init();
  page_init_high(ranges, nranges, reserved, nreserved);
  printk("memory: %lu MiB usable, %lu MiB free\n", total_pages >> 8, free_pages_count >> 8);
  arch_init();
  time_init();
  sched_init();
  pit_init(timer_tick);
  bcache_init();
  mem_devices_init();
  {
    char tym[128] = "";
    cmdline_get("tym", tym, sizeof(tym));
    tym_init(tym);
  }
  tty_init();
  kbd_init();
  serial_enable_irq();
  sti();
  ata_init();
  virtio_blk_init();
  vfs_init();
  mount_root_fs();
  proc_init_first();
  /* the first process starts in run_init, on its own kernel stack */
  struct task *t = list_entry(init_proc->threads.next, struct task, thread_link);
  uint64_t *sp = (uint64_t *)t->regs;
  *--sp = (uint64_t)kthread_start;
  *--sp = 0;                    /* rbx */
  *--sp = 0;                    /* rbp */
  *--sp = (uint64_t)run_init;   /* r12 */
  *--sp = 0;                    /* r13 */
  *--sp = 0;                    /* r14 */
  *--sp = 0;                    /* r15 */
  t->ksp = (uint64_t)sp;
  cli();
  make_runnable(t);
  sti();
  kthread_create("flush", flush_thread, NULL);
  {
    char ip[64] = "dhcp", loss[16] = "";
    cmdline_get("ip", ip, sizeof(ip));
    cmdline_get("netloss", loss, sizeof(loss));
    net_init_conf(ip, loss);
  }
  cpu_idle();
}
