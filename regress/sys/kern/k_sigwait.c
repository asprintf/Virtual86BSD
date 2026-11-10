/* k_sigwait.c - sigtimedwait timeouts and SIGCHLD delivery */
#define _POSIX_C_SOURCE 202405L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void h(int s) { (void)s; }

int main(void) {
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGCHLD);
  sigprocmask(SIG_BLOCK, &set, NULL);
  struct sigaction sa;
  memset(&sa, 0, sizeof sa);
  sa.sa_handler = h;
  sigaction(SIGCHLD, &sa, NULL);
  struct timespec ts = {1, 0};
  siginfo_t si;
  printf("waiting 1 s\n");
  fflush(stdout);
  int r = sigtimedwait(&set, &si, &ts);
  printf("sigtimedwait: %d errno %d (want -1 %d)\n", r, errno, EAGAIN);
  pid_t c = fork();
  if (c == 0) {
    sleep(1);
    _exit(3);
  }
  ts.tv_sec = 5;
  r = sigtimedwait(&set, &si, &ts);
  printf("sigtimedwait: %d (want %d) pid %d status %d\n", r, SIGCHLD, (int)si.si_pid, si.si_status);
  int st;
  printf("waitpid %d %d\n", (int)waitpid(c, &st, 0), WEXITSTATUS(st));
  setpgid(0, 0);
  printf("setpgid done, pgrp %d\n", (int)getpgrp());
  return 0;
}
