/*
 * Copyright (c) 2026, Joost Orchidee Winter
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

/* init.c - the first process of a V86BSD system.
 *
 * The states, in the classic BSD way:
 *
 *   runcom      run /etc/rc with sh; failure -> single-user
 *   read_ttys   read /etc/ttys; no enabled line -> the console shell
 *   multi_user  one getty per enabled line, respawned when it ends
 *   single_user an sh on the console, as the leader of a new session
 *   shutdown    SIGHUP the sessions, run /etc/rc.shutdown, kill everything,
 *               reboot(2)
 *
 * Booting with -s (the kernel passes it for `-s' on its command line) starts
 * in single-user mode; leaving that shell goes on with /etc/rc.  Without an
 * /etc/ttys (or with no `on' line) init runs rc, then the console shell, and
 * powers the machine off when that shell exits.
 *
 * Signals (the same as OpenBSD's): HUP reread /etc/ttys, TERM kill everything
 * and go to single-user, INT reboot, USR1 halt, USR2 power off.
 *
 * Security:
 *  - every signal is blocked except inside sigsuspend(), so none is lost and
 *    the handlers only set flags;
 *  - /etc/rc, /etc/rc.shutdown and /etc/ttys are used only if they are
 *    regular files that group and others cannot write;
 *  - a tty name from /etc/ttys is one plain word (no '/', no leading '.')
 *    and must be a character device in /dev; the getty is an absolute path;
 *  - a getty gets a clean environment (PATH, TERM) and no descriptor of
 *    init's: stdio is /dev/null, it opens its own terminal;
 *  - every child resets signal dispositions and the signal mask;
 *  - init never signals process 0 or a negative pid other than the kill(-1)
 *    of the shutdown (the kernel spares init itself);
 *  - the console named by CONSOLE (from the kernel command line) must be
 *    /dev/<word>.
 *
 * The utmp entry of a session whose process ended is marked DEAD_PROCESS
 * (login writes the USER_PROCESS entry).  There is no wtmp.
 *
 * A getty that ends less than GETTY_SPACING seconds after it started is
 * followed by one that sleeps GETTY_SLEEP seconds first (a getty or login
 * that cannot run must not loop), unless a user had logged in on the line:
 * then the session was real, however short, and the next getty is at once.
 *
 * POSIX C, plus reboot(2) (<sys/reboot.h>).
 */
#define _XOPEN_SOURCE 800
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <utmpx.h>

#define PATH_RC "/etc/rc"
#define PATH_RCSHUTDOWN "/etc/rc.shutdown"
#define PATH_TTYS "/etc/ttys"
#define STD_PATH "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin"

#define MAXSESS 16
#define GETTY_SPACING 5 /* a getty restarted within this many seconds ... */
#define GETTY_SLEEP 30  /* ... sleeps this long before it runs */
#define DEATH_WATCH 10  /* seconds each kill round waits */
#define SCRIPT_WAIT 60  /* seconds rc.shutdown may take */
#define STALL 5

extern char **environ;

struct session {
  char dev[40];      /* the name of the tty, and /dev/<name> is its device */
  char getty[160];   /* the getty command (the tty name is added) */
  char type[32];     /* the terminal type, or "" */
  pid_t pid;         /* 0: no process */
  time_t started;    /* monotonic seconds of the last start, 0: never (or a login was done) */
  int shutdown;      /* do not restart; remove when the process ended */
  int present;       /* still in /etc/ttys (while it is read) */
};

static struct session sess[MAXSESS];
static int nsess;

static volatile sig_atomic_t got_hup, got_term, got_down, got_alarm, down_howto;
static sigset_t nosig;
static int no_children;

enum state { RUNCOM, READ_TTYS, MULTI_USER, SINGLE_USER, DEATH, DEATH_SINGLE };

static enum state after_single = RUNCOM; /* where the console shell leads */
static int single_poweroff;               /* after it: power off */
static char console_path[64] = "/dev/tty00";

/* what init complains about goes to the console (its stderr) and to
   syslogd (daemon.err; a message sent before syslogd runs is lost, the
   console has it) */
static void say(const char *s) {
  (void)!write(2, "init: ", 6);
  (void)!write(2, s, strlen(s));
  (void)!write(2, "\n", 1);
  syslog(LOG_ERR, "%s", s);
}

static void say2(const char *s, const char *t) {
  char b[160];
  snprintf(b, sizeof b, "%s: %s", s, t);
  say(b);
}

static void on_signal(int s) {
  switch (s) {
  case SIGHUP: got_hup = 1; break;
  case SIGTERM: got_term = 1; break;
  case SIGINT: down_howto = RB_AUTOBOOT; got_down = 1; break;
  case SIGUSR1: down_howto = RB_HALT; got_down = 1; break;
  case SIGUSR2: down_howto = RB_HALT | RB_POWERDOWN; got_down = 1; break;
  case SIGALRM: got_alarm = 1; break;
  default: break; /* SIGCHLD: only to end sigsuspend() */
  }
}

static void setup_signals(void) {
  static const int sigs[] = {SIGHUP, SIGTERM, SIGINT, SIGUSR1, SIGUSR2, SIGALRM, SIGCHLD};
  struct sigaction sa;
  sigset_t handled;
  memset(&sa, 0, sizeof sa);
  sa.sa_handler = on_signal;
  sigfillset(&sa.sa_mask);
  sigemptyset(&handled);
  for (unsigned i = 0; i < sizeof sigs / sizeof *sigs; i++) {
    sigaction(sigs[i], &sa, NULL);
    sigaddset(&handled, sigs[i]);
  }
  signal(SIGTSTP, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);
  sigemptyset(&nosig);
  sigprocmask(SIG_BLOCK, &handled, NULL);
}

/* in a child, before it runs another program */
static void child_prepare(void) {
  sigset_t none;
  sigemptyset(&none);
  sigprocmask(SIG_SETMASK, &none, NULL);
  /* a shell keeps what was ignored at its start ignored for its jobs */
  for (int s = 1; s < 32; s++)
    if (s != SIGKILL && s != SIGSTOP)
      signal(s, SIG_DFL);
  umask(022);
}

static void close_fds(int from) {
  for (int fd = from; fd < 256; fd++)
    close(fd);
}

/* a regular file that only its owner (or root) can change: 1; missing: -1 */
static int safe_file(const char *path) {
  struct stat st;
  if (stat(path, &st) < 0)
    return -1;
  if (!S_ISREG(st.st_mode) || (st.st_mode & (S_IWGRP | S_IWOTH)))
    return 0;
  return 1;
}

static int valid_name(const char *s) {
  size_t n = strlen(s);
  if (n == 0 || n >= sizeof sess[0].dev - 5 || s[0] == '.')
    return 0;
  for (; *s; s++)
    if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') ||
          *s == '_' || *s == '.' || *s == '-'))
      return 0;
  return 1;
}

/* ---- sessions ---- */

/* mark the record of the session's process dead; 1 if it was a user's (a
   login had been done) */
static int clear_utmp(pid_t pid) {
  struct utmpx *u;
  int user = 0;
  setutxent();
  while ((u = getutxent()) != NULL) {
    if ((u->ut_type == USER_PROCESS || u->ut_type == LOGIN_PROCESS) && u->ut_pid == pid) {
      struct utmpx d = *u;
      user = u->ut_type == USER_PROCESS;
      d.ut_type = DEAD_PROCESS;
      memset(d.ut_user, 0, sizeof d.ut_user);
      memset(d.ut_host, 0, sizeof d.ut_host);
      d.ut_tv.tv_sec = time(NULL);
      d.ut_tv.tv_usec = 0;
      pututxline(&d);
      break;
    }
  }
  endutxent();
  return user;
}

static void remove_session(int i) {
  sess[i] = sess[--nsess];
}

/* a child that is not the one waited for: a session, or an orphan */
static void collect_session(pid_t pid) {
  for (int i = 0; i < nsess; i++) {
    if (sess[i].pid != pid)
      continue;
    /* a line that had a login is not one that fails to start: its next
       getty is not held back, however soon the user logged out */
    if (clear_utmp(pid))
      sess[i].started = 0;
    sess[i].pid = 0;
    if (sess[i].shutdown)
      remove_session(i);
    return;
  }
}

/* reap every child that has ended; 1 if `want' was one of them */
static int reap(pid_t want, int *wst) {
  int found = 0, st;
  pid_t p;
  no_children = 0;
  while ((p = waitpid(-1, &st, WNOHANG)) != 0) {
    if (p < 0) {
      if (errno == EINTR)
        continue;
      if (errno == ECHILD)
        no_children = 1;
      break;
    }
    if (p == want) {
      found = 1;
      if (wst)
        *wst = st;
    } else
      collect_session(p);
  }
  return found;
}

#define W_SIG 1 /* also return when a halt, reboot or power-off is requested */

/* wait until `want' ended (1), or there is no child, the timeout (seconds, 0:
   none) or, with W_SIG, a request ended the wait (0) */
static int wait_for(pid_t want, int *wst, int flags, unsigned timeout) {
  int r = 0;
  got_alarm = 0;
  if (timeout)
    alarm(timeout);
  for (;;) {
    if (reap(want, wst)) {
      r = 1;
      break;
    }
    if (no_children || got_alarm)
      break;
    if ((flags & W_SIG) && got_down)
      break;
    sigsuspend(&nosig);
  }
  if (timeout) {
    sigset_t pend, old;
    alarm(0);
    /* a SIGALRM that is already pending must not end the next wait */
    sigpending(&pend);
    if (sigismember(&pend, SIGALRM)) {
      sigprocmask(SIG_SETMASK, &nosig, &old);
      sigprocmask(SIG_SETMASK, &old, NULL);
    }
  }
  got_alarm = 0;
  return r;
}

static void monotonic(time_t *t) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  *t = ts.tv_sec;
}

static void start_getty(struct session *s) {
  struct stat st;
  char dev[64];
  snprintf(dev, sizeof dev, "/dev/%s", s->dev);
  if (stat(dev, &st) < 0 || !S_ISCHR(st.st_mode)) {
    say2(dev, "no such terminal, line disabled");
    s->shutdown = 1; /* a SIGHUP rereads /etc/ttys and tries again */
    return;
  }
  time_t now;
  monotonic(&now);
  int too_fast = s->started && now - s->started < GETTY_SPACING;
  pid_t pid = fork();
  if (pid < 0) {
    say("fork failed");
    return;
  }
  if (pid == 0) {
    char cmd[sizeof s->getty + sizeof s->dev + 2], term[48], pathenv[sizeof "PATH=" + sizeof STD_PATH];
    char *argv[12], *envp[3], *save, *w;
    int n = 0;
    child_prepare();
    if (too_fast) {
      say2(dev, "getty repeating too quickly, sleeping");
      sleep(GETTY_SLEEP);
    }
    snprintf(cmd, sizeof cmd, "%s %s", s->getty, s->dev);
    for (w = strtok_r(cmd, " \t", &save); w && n < 11; w = strtok_r(NULL, " \t", &save))
      argv[n++] = w;
    argv[n] = NULL;
    snprintf(pathenv, sizeof pathenv, "PATH=%s", STD_PATH);
    n = 0;
    envp[n++] = pathenv;
    if (s->type[0] && strcmp(s->type, "unknown")) {
      snprintf(term, sizeof term, "TERM=%s", s->type);
      envp[n++] = term;
    }
    envp[n] = NULL;
    /* nothing of init's: stdio is /dev/null, the getty opens its tty */
    close_fds(0);
    if (open("/dev/null", O_RDWR) == 0) {
      dup2(0, 1);
      dup2(0, 2);
    }
    execve(argv[0], argv, envp);
    _exit(127);
  }
  s->pid = pid;
  s->started = now;
}

/* ---- /etc/ttys ---- */

struct ttyent {
  char name[sizeof sess[0].dev];
  char getty[sizeof sess[0].getty];
  char type[sizeof sess[0].type];
  int on;
};

static int copy_word(char *dst, size_t n, const char *s, size_t len) {
  if (len >= n)
    return -1;
  memcpy(dst, s, len);
  dst[len] = 0;
  return 0;
}

/* name "getty command" type on|off [secure ...]; 1: a line, 0: none or bad */
static int parse_ttyent(char *p, struct ttyent *te) {
  char *q;
  memset(te, 0, sizeof *te);
  p += strspn(p, " \t");
  if (!*p || *p == '#' || *p == '\n')
    return 0;
  q = p + strcspn(p, " \t\n");
  if (copy_word(te->name, sizeof te->name, p, (size_t)(q - p)))
    return 0;
  p = q + strspn(q, " \t");
  if (*p == '"') {
    p++;
    q = strchr(p, '"');
    if (!q)
      return 0;
    if (copy_word(te->getty, sizeof te->getty, p, (size_t)(q - p)))
      return 0;
    p = q + 1;
  } else {
    q = p + strcspn(p, " \t\n");
    if (copy_word(te->getty, sizeof te->getty, p, (size_t)(q - p)))
      return 0;
    p = q;
  }
  p += strspn(p, " \t");
  q = p + strcspn(p, " \t\n");
  if (copy_word(te->type, sizeof te->type, p, (size_t)(q - p)))
    return 0;
  p = q;
  for (;;) { /* the status words: only `on' matters here */
    p += strspn(p, " \t");
    q = p + strcspn(p, " \t\n#");
    if (q == p)
      break; /* the end, or a comment */
    if (q - p == 2 && !strncmp(p, "on", 2))
      te->on = 1;
    p = q;
  }
  if (!valid_name(te->name) || te->getty[0] != '/')
    return 0;
  for (const char *t = te->type; *t; t++)
    if (!((*t >= 'a' && *t <= 'z') || (*t >= 'A' && *t <= 'Z') || (*t >= '0' && *t <= '9') ||
          *t == '_' || *t == '.' || *t == '-' || *t == '+'))
      return 0;
  return 1;
}

static struct session *find_session(const char *name) {
  for (int i = 0; i < nsess; i++)
    if (!strcmp(sess[i].dev, name))
      return &sess[i];
  return NULL;
}

/* (re)read /etc/ttys; the number of lines that stay enabled */
static int load_ttys(void) {
  char line[512];
  FILE *f = NULL;
  int ok = safe_file(PATH_TTYS);
  for (int i = 0; i < nsess; i++)
    sess[i].present = 0;
  if (ok == 0)
    say2(PATH_TTYS, "not a regular file, or writable by others: ignored");
  if (ok == 1)
    f = fopen(PATH_TTYS, "r");
  while (f && fgets(line, sizeof line, f)) {
    struct ttyent te;
    struct session *s;
    if (!strchr(line, '\n') && !feof(f)) { /* too long: skip the rest of it */
      int c;
      while ((c = getc(f)) != EOF && c != '\n')
        ;
      continue;
    }
    if (!parse_ttyent(line, &te))
      continue;
    s = find_session(te.name);
    if (!s) {
      if (!te.on || nsess >= MAXSESS)
        continue;
      s = &sess[nsess++];
      memset(s, 0, sizeof *s);
      memcpy(s->dev, te.name, sizeof s->dev);
    }
    memcpy(s->getty, te.getty, sizeof s->getty);
    memcpy(s->type, te.type, sizeof s->type);
    s->present = 1;
    s->shutdown = !te.on;
  }
  if (f)
    fclose(f);
  for (int i = nsess - 1; i >= 0; i--) {
    if (!sess[i].present)
      sess[i].shutdown = 1;
    if (sess[i].shutdown && sess[i].pid > 0)
      kill(sess[i].pid, SIGHUP); /* never pid 0 */
    if (sess[i].shutdown && sess[i].pid == 0)
      remove_session(i);
  }
  int n = 0;
  for (int i = 0; i < nsess; i++)
    if (!sess[i].shutdown)
      n++;
  return n;
}

/* ---- running programs ---- */

static pid_t run_script(const char *path) {
  pid_t pid = fork();
  if (pid < 0) {
    say("fork failed");
    return -1;
  }
  if (pid == 0) {
    char *argv[] = {"sh", (char *)path, NULL};
    child_prepare();
    execve("/bin/sh", argv, environ);
    say2("cannot run /bin/sh", path);
    _exit(127);
  }
  return pid;
}

static pid_t start_shell(void) {
  pid_t pid = fork();
  if (pid < 0) {
    say("fork failed");
    return -1;
  }
  if (pid == 0) {
    char *argv[] = {"/bin/sh", NULL};
    child_prepare();
    close_fds(3);
    setsid();
    int fd = open(console_path, O_RDWR); /* becomes the controlling terminal */
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
    execve("/bin/sh", argv, environ);
    say("cannot run the shell");
    _exit(127);
  }
  return pid;
}

/* ---- the states ---- */

static enum state runcom(void) {
  int st = 0, sf = safe_file(PATH_RC);
  pid_t pid;
  if (sf < 0)
    return READ_TTYS;
  if (sf == 0) {
    say2(PATH_RC, "not a regular file, or writable by others: not run");
    after_single = RUNCOM;
    return SINGLE_USER;
  }
  pid = run_script(PATH_RC);
  if (pid < 0 || !wait_for(pid, &st, 0, 0) || !WIFEXITED(st) || WEXITSTATUS(st) != 0) {
    say("/etc/rc failed: single-user mode");
    after_single = RUNCOM;
    return SINGLE_USER;
  }
  return READ_TTYS;
}

static enum state read_ttys(void) {
  if (load_ttys() == 0) {
    single_poweroff = 1; /* no terminal lines: the console shell, then off */
    return SINGLE_USER;
  }
  return MULTI_USER;
}

static enum state multi_user(void) {
  got_hup = got_term = 0;
  for (;;) {
    /* reap first: a session that just ended is started again below */
    reap(0, NULL);
    for (int i = 0; i < nsess; i++)
      if (sess[i].pid == 0 && !sess[i].shutdown)
        start_getty(&sess[i]);
    for (int i = nsess - 1; i >= 0; i--) /* lines disabled by start_getty */
      if (sess[i].shutdown && sess[i].pid == 0)
        remove_session(i);
    if (got_down)
      return DEATH;
    if (got_term) {
      got_term = 0;
      return DEATH_SINGLE;
    }
    if (got_hup) {
      got_hup = 0;
      load_ttys();
      continue; /* start what is new */
    }
    if (nsess == 0) {
      single_poweroff = 1;
      return SINGLE_USER;
    }
    sigsuspend(&nosig); /* nothing is lost: the signals were blocked */
  }
}

static enum state single_user(void) {
  int st = 0;
  pid_t pid;
  got_hup = got_term = 0; /* already there */
  pid = start_shell();
  if (pid < 0 || !wait_for(pid, &st, W_SIG, 0)) {
    if (got_down)
      return DEATH;
    sleep(STALL);
    return SINGLE_USER;
  }
  got_hup = got_term = 0;
  if (WIFEXITED(st) && WEXITSTATUS(st) == 127)
    sleep(STALL); /* the shell could not be started: not a tight loop */
  if (single_poweroff) {
    down_howto = RB_HALT | RB_POWERDOWN;
    return DEATH;
  }
  return after_single;
}

/* SIGHUP, SIGTERM and SIGKILL to everyone but init, 10 s of waiting after each */
static void kill_all(void) {
  static const int rounds[] = {SIGHUP, SIGTERM, SIGKILL};
  for (unsigned i = 0; i < sizeof rounds / sizeof *rounds; i++) {
    if (kill(-1, rounds[i]) < 0 && errno == ESRCH)
      return;
    wait_for(0, NULL, 0, DEATH_WATCH);
    if (kill(-1, 0) < 0 && errno == ESRCH)
      return; /* nobody left */
  }
  say("some processes would not die");
}

static void shutdown_sessions(void) {
  for (int i = 0; i < nsess; i++) {
    sess[i].shutdown = 1;
    if (sess[i].pid > 0)
      kill(sess[i].pid, SIGHUP);
  }
}

static enum state death(void) {
  int st;
  int howto = (int)down_howto;
  shutdown_sessions();
  syslog(LOG_NOTICE, "going down: %s",
         howto & RB_POWERDOWN ? "power off" : howto & RB_HALT ? "halt" : "reboot");
  if (safe_file(PATH_RCSHUTDOWN) == 1) {
    pid_t pid = run_script(PATH_RCSHUTDOWN);
    if (pid > 0 && !wait_for(pid, &st, 0, SCRIPT_WAIT)) {
      say("/etc/rc.shutdown did not finish");
      kill(pid, SIGKILL);
      wait_for(pid, &st, 0, STALL);
    }
  } else if (safe_file(PATH_RCSHUTDOWN) == 0)
    say2(PATH_RCSHUTDOWN, "not a regular file, or writable by others: not run");
  kill_all();
  sync();
  reboot(howto);
  say("reboot failed");
  got_down = 0;
  single_poweroff = 0;
  after_single = RUNCOM;
  return SINGLE_USER;
}

static enum state death_single(void) {
  shutdown_sessions();
  kill_all();
  after_single = RUNCOM;
  single_poweroff = 0;
  return SINGLE_USER;
}

int main(int argc, char **argv) {
  enum state s = RUNCOM;
  if (getuid() != 0 || getpid() != 1) {
    say("must be process 1, run by root");
    return 1;
  }
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-s")) {
      s = SINGLE_USER;
      after_single = RUNCOM;
    } else
      say2("unknown argument", argv[i]);
  }
  setup_signals();
  openlog("init", LOG_ODELAY, LOG_DAEMON);
  setenv("PATH", STD_PATH, 1);
  setenv("HOME", "/root", 1);
  setenv("SHELL", "/bin/sh", 1);
  setenv("USER", "root", 1);
  setenv("LOGNAME", "root", 1);
  if (!getenv("TERM"))
    setenv("TERM", "vt100", 1);
  umask(022);
  if (chdir("/root") != 0)
    (void)!chdir("/");
  /* the console: /dev/<word> only */
  const char *c = getenv("CONSOLE");
  if (c && !strncmp(c, "/dev/", 5) && valid_name(c + 5) && strlen(c) < sizeof console_path)
    strcpy(console_path, c);
  unsetenv("CONSOLE");
  for (;;) {
    switch (s) {
    case RUNCOM: s = runcom(); break;
    case READ_TTYS: s = read_ttys(); break;
    case MULTI_USER: s = multi_user(); break;
    case SINGLE_USER:
      setenv("ENV", "/etc/profile", 0); /* the shell reads it */
      s = single_user();
      break;
    case DEATH: s = death(); break;
    case DEATH_SINGLE: s = death_single(); break;
    }
  }
}
