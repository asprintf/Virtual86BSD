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
 * write.c - write: write to another user (POSIX.1-2024, XCU write)
 *
 * Synopsis: write user_name [terminal]
 *
 * The recipient's terminals are the lines of user_name in the user
 * accounting database (utx.c), and their files in /dev.  Messages are
 * allowed on a terminal the group or others may write to (mesg y); a
 * process with an effective user ID of 0 may write to any.  The first
 * message is
 *     \r\n\aMessage from SENDER (TTY) [Thu Oct 29 13:56:02 2026]...\r\n
 * with the sender's login name and terminal (??? when there is none),
 * and the sender's terminal is then alerted twice.  Each line, or what a
 * read of the standard input returns, is written as it comes: printable
 * characters and the space class as they are, <alert> as it is, newline
 * as \r\n; the other control characters as ^X (^? for DEL), and the
 * other bytes (not a printable character of the locale) as M- and the
 * same for the byte less its high bit.  At end of file, or on an
 * interrupt, "EOT\r\n" is written (on a line of its own) and write exits
 * with status 0.
 *
 * Choices where the standard leaves room:
 *  - An interrupt ignored when write starts stays ignored.
 *  - Of several terminals, the one with the latest access (the least
 *    idle) that allows messages, not the sender's own unless it is the
 *    only one; the message on the standard output names it.
 *  - The terminal operand may be given as in who ("pts/1") or as a
 *    pathname in /dev ("/dev/pts/1").
 *  - The user not logged on, or on that terminal, or the terminals not
 *    allowing messages: a message and status 1, as for any error;
 *    usage errors, status 2.
 *  - POSIX_UTMPX and POSIX_DEVDIR name another database and directory of
 *    terminals (hooks for the tests, see utx.c).
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include "utx.h"

#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <locale.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utmpx.h>
#include <wchar.h>
#include <wctype.h>

static volatile sig_atomic_t interrupted;
static int tfd = -1;            /* the recipient's terminal */

static void on_int(int sig)
{
    (void)sig;
    interrupted = 1;
}

static bool put(const char *s, size_t n)
{
    while (n > 0) {
        ssize_t k = write(tfd, s, n);
        if (k < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        s += k;
        n -= (size_t)k;
    }
    return true;
}

static bool midline;             /* the last written was not a newline */

static void finish(int status)
{
    if (((midline && !put("\r\n", 2)) || !put("EOT\r\n", 5)) && status == 0) {
        fprintf(stderr, "write: %s\n", strerror(errno));
        status = 1;
    }
    exit(status);
}

/* the sender's terminal: its descriptor, or -1 */
static int my_tty(void)
{
    for (int fd = 0; fd < 3; fd++)
        if (isatty(fd))
            return fd;
    return -1;
}

/* the converted form of buf[0..n) in out (of room 8n + 1); *used: the
   bytes taken, short of an incomplete character at the end unless final */
static size_t convert(const char *buf, size_t n, char *out, mbstate_t *ps, size_t *used, bool final)
{
    size_t o = 0, i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)buf[i];
        if (c == '\n') {
            out[o++] = '\r';
            out[o++] = '\n';
            i++;
            continue;
        }
        if (c == '\a') {
            out[o++] = '\a';
            i++;
            continue;
        }
        wchar_t wc;
        size_t k = mbrtowc(&wc, buf + i, n - i, ps);
        if (k == (size_t)-2) {
            memset(ps, 0, sizeof *ps);
            if (!final)
                break;          /* the rest comes with the next read */
            k = (size_t)-1;
        }
        if (k != (size_t)-1 && k != 0 && (iswprint((wint_t)wc) || iswspace((wint_t)wc))) {
            memcpy(out + o, buf + i, k);
            o += k;
            i += k;
            continue;
        }
        if (k == (size_t)-1)
            memset(ps, 0, sizeof *ps);
        /* one byte, made printable */
        if (c >= 0x80) {
            out[o++] = 'M';
            out[o++] = '-';
            c &= 0x7f;
        }
        if (c < 0x20) {
            out[o++] = '^';
            out[o++] = (char)(c + '@');
        } else if (c == 0x7f) {
            out[o++] = '^';
            out[o++] = '?';
        } else {
            out[o++] = (char)c;
        }
        i++;
    }
    *used = i;
    return o;
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    int a = 1;
    if (a < argc && strcmp(argv[a], "--") == 0)
        a++;
    if (argc - a < 1 || argc - a > 2 || (argv[a][0] == '-' && argv[a][1] != '\0')) {
        fprintf(stderr, "usage: write user_name [terminal]\n");
        return 2;
    }
    const char *user = argv[a];
    const char *want = argc - a == 2 ? argv[a + 1] : NULL;
    if (want) {
        if (strncmp(want, "/dev/", 5) == 0)
            want += 5;
        else
            want = utx_linename(want);
    }

    /* the sender */
    const char *me = getlogin();
    if (!me || !*me) {
        struct passwd *pw = getpwuid(getuid());
        me = pw ? pw->pw_name : "???";
    }
    int myfd = my_tty();
    char myline[1024] = "???";
    if (myfd >= 0) {
        const char *n = ttyname(myfd);
        if (n)
            snprintf(myline, sizeof myline, "%s", utx_linename(n));
    }

    /* the recipient's terminal */
    if (!utx_open(NULL)) {
        fprintf(stderr, "write: the user accounting database: %s\n", strerror(errno));
        return 1;
    }
    struct utx u;
    int logged = 0;
    char best[300] = "", self[300] = "";
    time_t bestat = 0;
    while (utx_next(&u)) {
        if (u.type != USER_PROCESS || strcmp(u.user, user) != 0)
            continue;
        if (want && strcmp(u.line, want) != 0)
            continue;
        logged++;
        char *p = utx_ttypath(u.line);
        struct stat st;
        bool ok = p && stat(p, &st) == 0 && ((st.st_mode & (S_IWGRP | S_IWOTH)) || geteuid() == 0);
        free(p);
        if (!ok)
            continue;
        if (strcmp(u.line, myline) == 0 && strcmp(user, me) == 0) {
            snprintf(self, sizeof self, "%s", u.line);
            continue;
        }
        if (!best[0] || st.st_atime > bestat) {
            snprintf(best, sizeof best, "%s", u.line);
            bestat = st.st_atime;
        }
    }
    utx_close();
    if (!best[0] && self[0])
        snprintf(best, sizeof best, "%s", self);
    if (logged == 0) {
        if (want)
            fprintf(stderr, "write: %s is not logged on %s\n", user, want);
        else
            fprintf(stderr, "write: %s is not logged on\n", user);
        return 1;
    }
    if (!best[0]) {
        fprintf(stderr, "write: %s has messages disabled\n", user);
        return 1;
    }
    if (logged > 1)
        printf("write: %s is logged on more than one place.\nYou are connected to \"%s\".\n", user, best);
    fflush(stdout);

    char *path = utx_ttypath(best);
    tfd = path ? open(path, O_WRONLY | O_NOCTTY | O_APPEND) : -1;
    if (tfd < 0) {
        fprintf(stderr, "write: %s: %s\n", path ? path : best, strerror(errno));
        return 1;
    }
    free(path);

    struct sigaction sa, old;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_int;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, NULL, &old) == 0 && old.sa_handler != SIG_IGN)
        sigaction(SIGINT, &sa, NULL);

    char date[64], head[2048];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if (!tm || strftime(date, sizeof date, "%a %b %e %H:%M:%S %Y", tm) == 0)
        snprintf(date, sizeof date, "?");
    int hl = snprintf(head, sizeof head, "\r\n\aMessage from %s (%s) [%s]...\r\n", me, myline, date);
    if (!put(head, (size_t)hl)) {
        fprintf(stderr, "write: %s\n", strerror(errno));
        return 1;
    }
    if (myfd >= 0) {
        ssize_t r = write(myfd, "\a\a", 2);
        (void)r;
    }

    char buf[4096 + MB_LEN_MAX], out[8 * (4096 + MB_LEN_MAX) + 1];
    size_t have = 0;            /* an incomplete character kept from the last read */
    mbstate_t ps;
    memset(&ps, 0, sizeof ps);
    for (;;) {
        if (interrupted)
            break;
        ssize_t n = read(0, buf + have, 4096);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "write: standard input: %s\n", strerror(errno));
            finish(1);
        }
        size_t used, total = have + (size_t)n;
        size_t o = convert(buf, total, out, &ps, &used, n == 0);
        if (!put(out, o)) {
            fprintf(stderr, "write: %s\n", strerror(errno));
            return 1;
        }
        if (o > 0)
            midline = out[o - 1] != '\n';
        have = total - used;
        if (have > MB_LEN_MAX)          /* cannot be a character: as bytes */
            have = 0;
        memmove(buf, buf + used, have);
        if (n == 0)
            break;
    }
    finish(0);
}
