/* init.c - the first process of a Shall system.
 *
 * Runs /etc/rc (if there is one) with sh, then an interactive sh on the
 * console terminal as the leader of a new session, so that it has a
 * controlling terminal and job control.  Reaps every orphan.  When the
 * console shell exits, init syncs the disks and powers the machine off.
 *
 * The terminal: the kernel's console= parameter is not visible here, so
 * init takes CONSOLE from its environment (the kernel passes unknown
 * name=value words of its command line on as environment, as Linux does),
 * default /dev/ttyS0.
 *
 * POSIX C, plus reboot(2) of Linux for the power-off (poweroff.s: Shall's
 * C library has no reboot()).
 */
#define _XOPEN_SOURCE 800
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

long linux_poweroff(void); /* poweroff.s */

static void say(const char *s) {
  (void)!write(2, "init: ", 6);
  (void)!write(2, s, strlen(s));
  (void)!write(2, "\n", 1);
}

static void poweroff(void) {
  say("syncing disks and powering off");
  sync();
  linux_poweroff();
  for (;;)
    pause();
}

static pid_t start(const char *const argv[], const char *tty, int wait_for) {
  pid_t pid = fork();
  if (pid < 0) {
    say("fork failed");
    return -1;
  }
  if (pid == 0) {
    sigset_t none;
    sigemptyset(&none);
    sigprocmask(SIG_SETMASK, &none, NULL);
    /* default dispositions: a shell keeps signals that were ignored at
       its start ignored for its jobs */
    for (int s = 1; s < 32; s++)
      if (s != SIGKILL && s != SIGSTOP)
        signal(s, SIG_DFL);
    if (tty) {
      setsid();
      int fd = open(tty, O_RDWR); /* becomes the controlling terminal */
      if (fd < 0) {
        say("cannot open the console terminal");
        _exit(127);
      }
      dup2(fd, 0);
      dup2(fd, 1);
      dup2(fd, 2);
      if (fd > 2)
        close(fd);
      tcsetpgrp(0, getpid());
    }
    execv(argv[0], (char *const *)argv);
    say("cannot run the shell");
    _exit(127);
  }
  if (wait_for) {
    int st;
    while (waitpid(pid, &st, 0) < 0 && errno == EINTR)
      ;
  }
  return pid;
}

int main(void) {
  if (getpid() != 1) {
    say("must be process 1");
    return 1;
  }
  signal(SIGINT, SIG_IGN);
  signal(SIGTERM, SIG_IGN);
  signal(SIGHUP, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);
  setenv("PATH", "/usr/local/bin:/usr/bin:/bin", 0);
  setenv("HOME", "/root", 1);
  setenv("SHELL", "/bin/sh", 1);
  setenv("USER", "root", 1);
  setenv("LOGNAME", "root", 1);
  if (!getenv("TERM"))
    setenv("TERM", "vt100", 1);
  umask(022);
  if (chdir("/root") != 0)
    (void)!chdir("/");
  const char *tty = getenv("CONSOLE");
  if (!tty)
    tty = "/dev/ttyS0";
  struct stat st;
  if (stat("/etc/rc", &st) == 0) {
    const char *rc[] = {"/bin/sh", "/etc/rc", NULL};
    start(rc, NULL, 1);
  }
  const char *shell[] = {"/bin/sh", NULL};
  setenv("ENV", "/etc/profile", 0);
  pid_t sh = start(shell, tty, 0);
  for (;;) {
    int status;
    pid_t p = wait(&status);
    if (p < 0) {
      if (errno == ECHILD)
        poweroff();
      continue;
    }
    if (p == sh)
      poweroff();
  }
}
