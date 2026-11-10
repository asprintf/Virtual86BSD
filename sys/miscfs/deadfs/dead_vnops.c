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

/* dead_vnops.c - revoke(2): a device taken away from the files that have it
   open.
 *
 * revoke_dev(ino) turns every open file of the device ino (the same type and
 * device number, so also another name of it: tty00 and cua00) into a dead
 * file, in every process, whatever descriptor holds it and however many
 * descriptors share it (dup, fork).  A dead file keeps its descriptor and its
 * place in the table, and
 *   read   is end-of-file on a terminal, else EIO;
 *   write  fails with EIO;
 *   ioctl  fails with EBADF;
 *   poll   reports POLLHUP (and POLLIN: the read will not block);
 *   close  works, and does what closing the device did (the old release).
 * Whoever sleeps in a read or write of the device is woken, and gets that
 * result.  The terminals also hang up the session that has the terminal
 * (SIGHUP and SIGCONT to its foreground group, and it loses its controlling
 * terminal), as a hangup does.  A later open of the device is a new file,
 * not revoked.
 *
 * Not touched (as in BSD): memory mapped from the device, and a descriptor
 * in flight in a socket's queue (SCM_RIGHTS).
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/file.h>

static ssize_t dead_read(struct file *f, char *buf, size_t n, off_t *pos) {
  (void)f;
  (void)buf;
  (void)n;
  (void)pos;
  return -EIO;
}

static ssize_t dead_tty_read(struct file *f, char *buf, size_t n, off_t *pos) {
  (void)f;
  (void)buf;
  (void)n;
  (void)pos;
  return 0; /* end-of-file, as for a hung-up terminal */
}

static ssize_t dead_write(struct file *f, const char *buf, size_t n, off_t *pos) {
  (void)f;
  (void)buf;
  (void)n;
  (void)pos;
  return -EIO;
}

static long dead_ioctl(struct file *f, unsigned cmd, uint64_t arg) {
  (void)f;
  (void)cmd;
  (void)arg;
  return -EBADF;
}

static unsigned dead_poll(struct file *f) {
  (void)f;
  return POLLHUP | POLLIN | POLLRDNORM;
}

/* what closing the device did: the release it had before it was revoked */
static void dead_release(struct file *f) {
  if (f->rfops && f->rfops->release)
    f->rfops->release(f);
}

static const struct file_ops dead_ops = {
    .read = dead_read, .write = dead_write, .ioctl = dead_ioctl, .poll = dead_poll, .release = dead_release};

static const struct file_ops dead_tty_ops = {
    .read = dead_tty_read, .write = dead_write, .ioctl = dead_ioctl, .poll = dead_poll, .release = dead_release};

static bool same_device(const struct inode *a, const struct inode *b) {
  return (a->mode & S_IFMT) == (b->mode & S_IFMT) && a->rdev == b->rdev;
}

int revoke_dev(struct inode *ino) {
  list_for_each(q, &all_procs) {
    struct proc *p = list_entry(q, struct proc, all_link);
    if (p->state == P_DEAD || !p->fd)
      continue;
    for (int i = 0; i < p->nfd; i++) {
      struct file *f = p->fd[i];
      if (!f || !f->ino || file_revoked(f) || !same_device(f->ino, ino))
        continue;
      bool tty = tty_file_p(f);
      f->rfops = f->fops;
      f->fops = tty ? &dead_tty_ops : &dead_ops;
      if (tty)
        tty_file_revoked(f); /* wake the sleepers, hang up the session */
    }
  }
  return 0;
}
