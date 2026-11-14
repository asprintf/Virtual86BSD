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

/* init_main.c - kmain: the machine-independent start-up, from the clocks to
   the first process (init_x86_64 has set up the machine). */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/reboot.h>
#include <uvm/uvm.h>
#include <net/if_var.h>
#include <machine/cpu.h>

char kernel_cmdline[512];
int boothowto;

long flush_thread(void *arg);
NORETURN void cpu_idle(void);
void enter_user(struct regs *r);

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

/* the loader's arguments: the words `-letters' are the boot flags of
   <sys/reboot.h>, the rest (name=value, ro, rw) is read by cmdline_get */
void bootargs_init(const char *args) {
  size_t n = strnlen(args, sizeof(kernel_cmdline) - 1);
  memcpy(kernel_cmdline, args, n);
  kernel_cmdline[n] = 0;
  boothowto = 0;
  for (const char *s = kernel_cmdline; *s;) {
    while (*s == ' ')
      s++;
    if (*s == '-') {
      for (s++; *s && *s != ' '; s++) {
        if (*s == 's')
          boothowto |= RB_SINGLE;
        else
          printf("boot flag -%c is not supported\n", *s);
      }
    }
    while (*s && *s != ' ')
      s++;
  }
}

/* the root: root= on the command line, else the `root on' of the
   configuration (GENERIC: disk0s1), else the first disk (an image with no
   partition table) */
extern const char cf_rootdev[]; /* ioconf.c */

static void mount_root_fs(void) {
  char root[64] = "", type[32] = "ffs";
  bool given = cmdline_get("root", root, sizeof(root)) && root[0];
  if (!given)
    snprintf(root, sizeof(root), "%s", cf_rootdev);
  cmdline_get("rootfstype", type, sizeof(type));
  const char *name = !strncmp(root, "/dev/", 5) ? root + 5 : root;
  struct blockdev *b = name[0] ? blockdev_by_name(name) : NULL;
  if (!b && !given)
    b = blockdev_nth(0); /* (the configured one is not there: the first disk) */
  int e = -ENODEV;
  if (b) {
    name = b->name;
    e = mount_root(type, b);
    /* an FFS root stays read-only until /etc/rc has run fsck and then
       `mount -uw /'; `rw' on the command line says otherwise */
    bool ro = cmdline_flag("ro") || (!cmdline_flag("rw") && !strcmp(type, "ffs"));
    if (!e && ro)
      root_mount->sb->rdonly = true, root_mount->flags |= MS_RDONLY;
  }
  if (e) {
    printf("cannot mount root from %s, error = %d\n", name[0] ? name : "(none)", -e);
    if (mount_root("tmpfs", NULL))
      panic("no root file system");
    printf("root on tmpfs\n");
    return;
  }
  printf("root on %s (%s%s)\n", name, type, root_mount->sb->rdonly ? ", read-only" : "");
}

static void auto_mount(const char *dir, const char *type) {
  int e = do_mount(type, dir, type, 0, NULL);
  if (e && e != -ENOENT)
    printf("cannot mount %s on %s: error %d\n", type, dir, -e);
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
    printf("warning: unable to open an initial console\n");
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
  bool single = (boothowto & RB_SINGLE) != 0;
  const char *tries[] = {NULL, "/sbin/init", "/etc/init", "/bin/init", "/bin/sh"};
  if (cmdline_get("init", init, sizeof(init)))
    tries[0] = init;
  for (size_t i = 0; i < ARRAY_SIZE(tries); i++) {
    if (!tries[i])
      continue;
    /* `-s' on the command line: init starts in single-user mode */
    const char *argv[] = {tries[i], single && strcmp(tries[i], "/bin/sh") ? "-s" : NULL, NULL};
    int e = kernel_execve(tries[i], argv, (const char *const *)envp);
    if (e == 0) {
      cli();
      enter_user(current->regs);
    }
    if (e != -ENOENT)
      printf("cannot run %s: error %d\n", tries[i], -e);
  }
  panic("no init");
}

void kmain(void) {
  time_init();
  sched_init();
  cpu_initclocks();
  bcache_init();
  mem_devices_init();
  {
    char tym[128] = "";
    cmdline_get("tym", tym, sizeof(tym));
    tym_init(tym);
  }
  tty_init();
  cpu_configure(); /* the devices: mainbus0 and all it finds, as config(8) says */
  tym_report();
  sti();
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
  loadavg_init();
  {
    char ip[64] = "dhcp", loss[16] = "";
    cmdline_get("ip", ip, sizeof(ip));
    cmdline_get("netloss", loss, sizeof(loss));
    net_init_conf(ip, loss);
  }
  cpu_idle();
}
