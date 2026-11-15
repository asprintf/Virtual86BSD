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

/* the loader's arguments: the first word is the kernel (the loader's
   `disk0s1:/bsd', QEMU's file name), the rest are the boot flags of
   <sys/reboot.h>, `-letters' as the BSDs have them.  A letter or a word the
   kernel does not know is reported and ignored. */
void bootargs_init(const char *args) {
  size_t n = strnlen(args, sizeof(kernel_cmdline) - 1);
  memcpy(kernel_cmdline, args, n);
  kernel_cmdline[n] = 0;
  boothowto = 0;
  const char *s = kernel_cmdline;
  while (*s && *s != ' ')
    s++;
  while (*s) {
    while (*s == ' ')
      s++;
    if (*s == '-') {
      for (s++; *s && *s != ' '; s++) {
        if (*s == 's')
          boothowto |= RB_SINGLE;
        else
          printf("boot flag -%c is not supported\n", *s);
      }
    } else if (*s) {
      const char *w = s;
      while (*s && *s != ' ')
        s++;
      printf("boot argument %.*s is not supported\n", (int)(s - w), w);
    }
  }
}

/* the root: the disk (and partition) the loader booted from, else the
   `root on' of the configuration (GENERIC: disk0s1), else the first disk
   (an image with no partition table).  The file system type is what is
   there, FFS or ext2; it is mounted read-only, and /etc/rc checks it
   (fsck) and makes it writable. */
extern const char cf_rootdev[]; /* ioconf.c */

static void mount_root_fs(void) {
  char root[64] = "";
  struct blockdev *b = NULL;
  if (boot_drive >= 0x80) {
    /* (the BIOS numbers its drives from 0x80, as the disks attach: disk0 is 0x80) */
    snprintf(root, sizeof(root), "disk%d", boot_drive - 0x80);
    if (boot_part >= 0)
      snprintf(root + strlen(root), sizeof(root) - strlen(root), "s%d", boot_part + 1);
    b = blockdev_by_name(root);
  }
  if (!b) {
    snprintf(root, sizeof(root), "%s", cf_rootdev);
    b = root[0] ? blockdev_by_name(root) : NULL;
  }
  if (!b)
    b = blockdev_nth(0);
  static const char *const types[] = {"ffs", "ext2"};
  const char *type = "";
  int e = -ENODEV;
  const char *name = b ? b->name : root;
  for (size_t i = 0; b && i < ARRAY_SIZE(types) && e; i++) {
    type = types[i];
    e = mount_root(type, b);
  }
  if (e) {
    printf("cannot mount root from %s, error = %d\n", name[0] ? name : "(none)", -e);
    if (mount_root("tmpfs", NULL))
      panic("no root file system");
    printf("root on tmpfs\n");
    return;
  }
  root_mount->sb->rdonly = true, root_mount->flags |= MS_RDONLY;
  printf("root on %s (%s, read-only)\n", name, type);
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
  /* init gets a bare environment, as the BSDs give it */
  const char *envp[] = {"HOME=/", "TERM=linux", NULL};
  bool single = (boothowto & RB_SINGLE) != 0;
  static const char *const tries[] = {"/sbin/init", "/sbin/oinit", "/sbin/init.bak"};
  for (size_t i = 0; i < ARRAY_SIZE(tries); i++) {
    /* `-s' on the command line: init starts in single-user mode */
    const char *argv[] = {tries[i], single ? "-s" : NULL, NULL};
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
  tym_init();
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
  net_init_conf();
  cpu_idle();
}
