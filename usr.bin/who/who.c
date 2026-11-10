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

/*
 * who.c - who: display who is on the system (POSIX.1-2024, XCU who)
 *
 * Synopsis: who [-mTu] [-abdHlprt] [file]
 *           who [-mu] -s [-bHlprt] [file]
 *           who -q [file]
 *           who am i
 *           who am I
 *
 * The entries are those of the user accounting database (utx.c: through
 * getutxent(), or from file, a file of struct utmpx records).  The state
 * and the idle time of a line come from the terminal's file in /dev: its
 * write permission for the group or others, and its time of last access.
 *
 * The output: the fields in columns,
 *     name     [S] line         time         [idle    pid] [comment] [exit]
 * the name in 8 columns, the line in 12, the time as date +"%b %e %H:%M"
 * would write it (in the locale of LC_TIME); with -T the state after the
 * name ("%s %c %s %s\n" of the standard, its spaces one or more blanks).
 * The idle time is "." for activity in the last minute, "old" after 24
 * hours, else hours:minutes;
 * the pid is the process ID of the entry, written with -u and for the
 * entries of -l, -p and -d.  The comment: "(host)" for a
 * user with a host, "id=ID" for the processes of -l, -p and -d; the exit
 * of -d: "term=T exit=E" where the C library has them.
 *
 * Choices where the standard leaves room:
 *  - Which entries: USER_PROCESS by default; with -b (BOOT_TIME, the line
 *    "system boot"), -d (DEAD_PROCESS), -l (LOGIN_PROCESS, the name
 *    LOGIN), -p (INIT_PROCESS), -r (the run level, where the system keeps
 *    it: the line "run-level N") and -t (OLD_TIME and NEW_TIME: the lines
 *    "old time" and "new time") only those, and the users too when -T or
 *    -u is given.  -a is -bdlprtTu.  -s turns -T off (the name, line and
 *    time only; -u still adds its fields).
 *  - The name is left empty for the boot, the run level, the clock and the
 *    dead.
 *  - -m and "am i": the entries of the line of the first of the standard
 *    input, output and error that is a terminal; none when there is none.
 *  - -q: the names on one line (empty with none), then "# users=N".
 *  - The state: + when the group or others may write to the terminal, -
 *    when not, ? when its file cannot be had; a space for entries that
 *    are not users.
 *  - Usage errors exit with status 2; a file that cannot be read, 1.
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include "utx.h"

#include <errno.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utmpx.h>

#if !defined(RUN_LVL) && defined(__linux__)
#define RUN_LVL 1
#endif

static bool o_b, o_d, o_H, o_l, o_m, o_p, o_q, o_r, o_s, o_t, o_T, o_u;
static time_t now;

static void usage(void)
{
    fprintf(stderr, "usage: who [-mTu] [-abdHlprt] [file]\n"
                    "       who [-mu] -s [-bHlprt] [file]\n"
                    "       who -q [file]\n"
                    "       who am i\n");
    exit(2);
}

/* the line of the invoker's terminal, or NULL */
static const char *my_line(void)
{
    for (int fd = 0; fd < 3; fd++) {
        if (!isatty(fd))
            continue;
        const char *n = ttyname(fd);
        if (n) {
            static char buf[4096];
            snprintf(buf, sizeof buf, "%s", utx_linename(n));
            return buf;
        }
    }
    return NULL;
}

static void timestr(time_t t, char *buf, size_t n)
{
    struct tm *tm = localtime(&t);
    if (!tm || strftime(buf, n, "%b %e %H:%M", tm) == 0)
        snprintf(buf, n, "?");
}

/* the state and idle time of a terminal line */
static char state_of(const char *line, char *idle, size_t n)
{
    snprintf(idle, n, "?");
    if (!*line)
        return '?';
    char *p = utx_ttypath(line);
    struct stat st;
    int r = p ? stat(p, &st) : -1;
    free(p);
    if (r != 0)
        return '?';
    double d = difftime(now, st.st_atime);
    if (d < 60)
        snprintf(idle, n, ".");
    else if (d >= 24 * 3600.0)
        snprintf(idle, n, "old");
    else {
        long m = (long)(d / 60);
        snprintf(idle, n, "%ld:%02ld", m / 60, m % 60);
    }
    return (st.st_mode & (S_IWGRP | S_IWOTH)) ? '+' : '-';
}

static void out(const char *name, char state, const char *line, time_t t, const char *idle,
                long pid, bool pidtoo, const char *comment, const char *exitf)
{
    char tb[128], b[2048];
    size_t n = 0;
    timestr(t, tb, sizeof tb);
#define ADD(...) (n += (size_t)snprintf(b + n, n < sizeof b ? sizeof b - n : 0, __VA_ARGS__), \
                  n = n < sizeof b ? n : sizeof b - 1)
    if (o_T)
        ADD("%-8s %c %-12s %s", name, state, line, tb);
    else
        ADD("%-8s %-12s %s", name, line, tb);
    if (o_u || pidtoo) {
        ADD(" %5s", idle && o_u ? idle : "");
        if (pid >= 0)
            ADD(" %6ld", pid);
        else
            ADD(" %6s", "");
    }
    if (comment && *comment)
        ADD(" %s", comment);
    if (exitf && *exitf)
        ADD(" %s", exitf);
#undef ADD
    while (n > 0 && b[n - 1] == ' ')
        n--;
    b[n] = '\0';
    puts(b);
}

static bool wanted_type(int type)
{
    bool sel = o_b || o_d || o_l || o_p || o_r || o_t;
    switch (type) {
    case USER_PROCESS: return !sel || o_u || o_T;
    case BOOT_TIME: return o_b;
    case DEAD_PROCESS: return o_d;
    case LOGIN_PROCESS: return o_l;
    case INIT_PROCESS: return o_p;
    case OLD_TIME: case NEW_TIME: return o_t;
#ifdef RUN_LVL
    case RUN_LVL: return o_r;
#endif
    default: return false;
    }
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    int c;
    while ((c = getopt(argc, argv, "abdHlmpqrstTu")) != -1) {
        switch (c) {
        case 'a': o_b = o_d = o_l = o_p = o_r = o_t = o_T = o_u = true; break;
        case 'b': o_b = true; break;
        case 'd': o_d = true; break;
        case 'H': o_H = true; break;
        case 'l': o_l = true; break;
        case 'm': o_m = true; break;
        case 'p': o_p = true; break;
        case 'q': o_q = true; break;
        case 'r': o_r = true; break;
        case 's': o_s = true; break;
        case 't': o_t = true; break;
        case 'T': o_T = true; break;
        case 'u': o_u = true; break;
        default: usage();
        }
    }
    const char *file = NULL;
    int n = argc - optind;
    if (n == 1) {
        file = argv[optind];
    } else if (n == 2 && strcmp(argv[optind], "am") == 0
               && (strcmp(argv[optind + 1], "i") == 0 || strcmp(argv[optind + 1], "I") == 0)) {
        o_m = true;
    } else if (n != 0) {
        usage();
    }
    if (o_s)
        o_T = false;
    now = time(NULL);

    if (!utx_open(file)) {
        fprintf(stderr, "who: %s: %s\n", file ? file : "the user accounting database", strerror(errno));
        return 1;
    }
    const char *mine = NULL;
    if (o_m && !o_q) {
        mine = my_line();
        if (!mine) {
            utx_close();
            return 0;
        }
    }

    if (o_H && !o_q) {
        if (o_T)
            printf("%-8s %c %-12s %-12s", "NAME", 'S', "LINE", "TIME");
        else
            printf("%-8s %-12s %-12s", "NAME", "LINE", "TIME");
        if (o_u)
            printf(" %5s %6s", "IDLE", "PID");
        printf(" COMMENT");
        if (o_d)
            printf(" EXIT");
        putchar('\n');
    }

    struct utx u;
    long users = 0;
    bool firstname = true;
    while (utx_next(&u)) {
        if (o_q) {
            if (u.type == USER_PROCESS) {
                printf("%s%s", firstname ? "" : " ", u.user);
                firstname = false;
                users++;
            }
            continue;
        }
        if (!wanted_type(u.type))
            continue;
        if (mine && strcmp(u.line, mine) != 0)
            continue;
        char idle[32], comment[320], exitf[64];
        comment[0] = exitf[0] = '\0';
        char state = ' ';
        switch (u.type) {
        case USER_PROCESS:
            state = state_of(u.line, idle, sizeof idle);
            if (u.host[0])
                snprintf(comment, sizeof comment, "(%s)", u.host);
            out(u.user, state, u.line, u.time, idle, (long)u.pid, false, comment, NULL);
            break;
        case LOGIN_PROCESS:
            state_of(u.line, idle, sizeof idle);
            snprintf(comment, sizeof comment, "id=%s", u.id);
            out("LOGIN", ' ', u.line, u.time, idle, (long)u.pid, true, comment, NULL);
            break;
        case INIT_PROCESS:
            snprintf(comment, sizeof comment, "id=%s", u.id);
            out(u.user, ' ', u.line, u.time, NULL, (long)u.pid, true, comment, NULL);
            break;
        case DEAD_PROCESS:
            snprintf(comment, sizeof comment, "id=%s", u.id);
            if (u.has_exit)
                snprintf(exitf, sizeof exitf, "term=%d exit=%d", u.term, u.exit);
            out("", ' ', u.line, u.time, NULL, (long)u.pid, true, comment, exitf);
            break;
        case BOOT_TIME:
            out("", ' ', "system boot", u.time, NULL, -1, false, NULL, NULL);
            break;
        case OLD_TIME:
            out("", ' ', "old time", u.time, NULL, -1, false, NULL, NULL);
            break;
        case NEW_TIME:
            out("", ' ', "new time", u.time, NULL, -1, false, NULL, NULL);
            break;
#ifdef RUN_LVL
        case RUN_LVL: {
            char lb[32];
            int lev = u.pid % 256;
            snprintf(lb, sizeof lb, "run-level %c", lev > ' ' && lev < 127 ? lev : '?');
            int prev = u.pid / 256;
            if (prev > ' ' && prev < 127)
                snprintf(comment, sizeof comment, "last=%c", prev);
            out("", ' ', lb, u.time, NULL, -1, false, comment, NULL);
            break;
        }
#endif
        }
    }
    int err = errno;
    utx_close();
    if (o_q) {
        putchar('\n');
        printf("# users=%ld\n", users);
    }
    int status = 0;
    if (err != 0) {
        fprintf(stderr, "who: %s: %s\n", file ? file : "the user accounting database", strerror(err));
        status = 1;
    }
    if (fflush(stdout) != 0 || ferror(stdout)) {
        fprintf(stderr, "who: standard output: %s\n", strerror(errno));
        status = 1;
    }
    return status;
}
