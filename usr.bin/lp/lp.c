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
 * lp.c - lp: send files to a printer (POSIX.1-2024, XCU lp)
 *
 * Synopsis: lp [-c] [-d dest] [-n copies] [-msw] [-o option]... [-t title] [file...]
 *
 * lp prints at once, as the standard's rationale allows: it waits for the
 * destination, writes the request to it, and exits when it is written,
 * so the files are not needed afterwards with or without -c.  The
 * destinations are in a configuration file, /etc/lp.conf (make can set
 * another: -DLP_CONF=...; the environment variable LP_CONF names another
 * when lp is not set-user-ID or set-group-ID).  Its lines:
 *
 *     printer NAME [nobanner] PATHNAME     a device (or file) written to
 *     printer NAME [nobanner] | COMMAND    a command (sh -c) given the
 *                                          request on its standard input
 *     class NAME MEMBER...                 the first member that is free
 *     default NAME
 *     lockdir DIRECTORY                    the lock files (default /tmp)
 *
 * and blank lines and lines starting with #.  Without the file, the
 * destination lp is /dev/lp0, when that is a character device.
 *
 * Exclusive access: the request is written while lp holds a lock (fcntl)
 * on the file lockdir/lp.NAME.lock of the printer, which also holds the
 * number of its last request; the request ID is NAME-NUMBER.  A command
 * is given LP_REQUEST, LP_DEST, LP_USER, LP_TITLE, LP_COPIES (always 1:
 * lp writes the copies) and LP_OPTIONS (the -o options, separated by
 * spaces) in its environment.
 *
 * What is written: a banner page (unless the printer is nobanner or -o
 * nobanner is given) of the lines "Request:", "User:", "Title:" (with -t),
 * "Files:" and "Date:" (%c, in the locale's LC_TIME and TZ), ended by a
 * form feed; then the copies, each the files one after the other, as
 * they are.
 *
 * Choices where the standard leaves room:
 *  - The destination is -d, else LPDEST, else PRINTER, else the default
 *    of the configuration, else its only printer; with none, or an
 *    unknown one, lp fails (status 1) and prints nothing.
 *  - -o nobanner and -o banner choose the banner page; for a printer that
 *    is a device, other options are errors, for a command they are passed
 *    on.
 *  - "-" is the standard input (read once, also when named twice).  A
 *    file that cannot be read is reported and left out, and the status
 *    is 1; when no file can be read, nothing is printed.
 *  - The request ID is written as "request id is ID (N file(s))".
 *  - -m mails the user (mailx -s SUBJECT USER) when the request is
 *    printed; -w writes "lp: request ID has been printed" on a terminal
 *    of the user's that is in the user accounting database and can be
 *    written (utx.c), else mails.
 *  - Usage errors exit with status 2.
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include "utx.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <utmpx.h>

#ifndef LP_CONF
#define LP_CONF "/etc/lp.conf"
#endif

static int status;

static void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) {
        fputs("lp: out of memory\n", stderr);
        exit(1);
    }
    return p;
}

static char *xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    return memcpy(xmalloc(n), s, n);
}

static bool set_id(void)
{
    return getuid() != geteuid() || getgid() != getegid();
}

/* ---- the configuration ---- */

struct dest {
    char *name;
    bool isclass;
    bool nobanner;
    char *device;               /* a printer: the pathname, or NULL */
    char *command;              /* or the command */
    char **members;             /* a class */
    int nmembers;
};
static struct dest *dests;
static int ndests;
static char *default_dest;
static char *lockdir = "/tmp";

static struct dest *find_dest(const char *name)
{
    for (int i = 0; i < ndests; i++)
        if (strcmp(dests[i].name, name) == 0)
            return &dests[i];
    return NULL;
}

static struct dest *add_dest(const char *name)
{
    dests = realloc(dests, (size_t)(ndests + 1) * sizeof *dests);
    if (!dests) {
        fputs("lp: out of memory\n", stderr);
        exit(1);
    }
    struct dest *d = &dests[ndests++];
    memset(d, 0, sizeof *d);
    d->name = xstrdup(name);
    return d;
}

static bool name_ok(const char *s)
{
    if (!*s)
        return false;
    for (; *s; s++)
        if (!isalnum((unsigned char)*s) && !strchr("._-", *s))
            return false;
    return true;
}

static void conf_error(const char *file, int lineno, const char *what)
{
    fprintf(stderr, "lp: %s: line %d: %s\n", file, lineno, what);
    exit(1);
}

/* the next word of *p (blanks skipped), or NULL */
static char *word(char **p)
{
    char *s = *p;
    while (*s == ' ' || *s == '\t')
        s++;
    if (!*s)
        return NULL;
    char *w = s;
    while (*s && *s != ' ' && *s != '\t')
        s++;
    if (*s)
        *s++ = '\0';
    *p = s;
    return w;
}

static void read_conf(void)
{
    const char *file = NULL;
    if (!set_id())
        file = getenv("LP_CONF");
    if (!file || !*file)
        file = LP_CONF;
    FILE *f = fopen(file, "r");
    if (!f) {
        if (errno != ENOENT) {
            fprintf(stderr, "lp: %s: %s\n", file, strerror(errno));
            exit(1);
        }
        struct stat st;
        if (stat("/dev/lp0", &st) == 0 && S_ISCHR(st.st_mode)) {
            struct dest *d = add_dest("lp");
            d->device = xstrdup("/dev/lp0");
            default_dest = d->name;
        }
        return;
    }
    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    int lineno = 0;
    while ((n = getline(&line, &cap, f)) >= 0) {
        lineno++;
        if (n > 0 && line[n - 1] == '\n')
            line[n - 1] = '\0';
        char *p = line;
        char *kw = word(&p);
        if (!kw || kw[0] == '#')
            continue;
        if (strcmp(kw, "printer") == 0) {
            char *name = word(&p);
            if (!name || !name_ok(name))
                conf_error(file, lineno, "a printer needs a name (letters, digits, '.', '_', '-')");
            if (find_dest(name))
                conf_error(file, lineno, "a destination named twice");
            struct dest *d = add_dest(name);
            while (*p == ' ' || *p == '\t')
                p++;
            if (strncmp(p, "nobanner", 8) == 0 && (p[8] == ' ' || p[8] == '\t')) {
                d->nobanner = true;
                p += 8;
                while (*p == ' ' || *p == '\t')
                    p++;
            }
            if (*p == '|') {
                p++;
                while (*p == ' ' || *p == '\t')
                    p++;
                if (!*p)
                    conf_error(file, lineno, "no command");
                d->command = xstrdup(p);
            } else {
                char *dev = word(&p);
                if (!dev || word(&p))
                    conf_error(file, lineno, "a printer needs one pathname or a | command");
                d->device = xstrdup(dev);
            }
        } else if (strcmp(kw, "class") == 0) {
            char *name = word(&p);
            if (!name || !name_ok(name))
                conf_error(file, lineno, "a class needs a name (letters, digits, '.', '_', '-')");
            if (find_dest(name))
                conf_error(file, lineno, "a destination named twice");
            struct dest *d = add_dest(name);
            d->isclass = true;
            char *m;
            while ((m = word(&p))) {
                d->members = realloc(d->members, (size_t)(d->nmembers + 1) * sizeof *d->members);
                if (!d->members) {
                    fputs("lp: out of memory\n", stderr);
                    exit(1);
                }
                d->members[d->nmembers++] = xstrdup(m);
            }
            if (d->nmembers == 0)
                conf_error(file, lineno, "a class needs members");
        } else if (strcmp(kw, "default") == 0) {
            char *name = word(&p);
            if (!name || word(&p))
                conf_error(file, lineno, "default needs one name");
            default_dest = xstrdup(name);
        } else if (strcmp(kw, "lockdir") == 0) {
            char *dir = word(&p);
            if (!dir || word(&p))
                conf_error(file, lineno, "lockdir needs one directory");
            lockdir = xstrdup(dir);
        } else {
            conf_error(file, lineno, "unknown keyword");
        }
    }
    free(line);
    if (ferror(f)) {
        fprintf(stderr, "lp: %s: %s\n", file, strerror(errno));
        exit(1);
    }
    fclose(f);
    /* the members of the classes are printers */
    for (int i = 0; i < ndests; i++)
        for (int k = 0; dests[i].isclass && k < dests[i].nmembers; k++) {
            struct dest *m = find_dest(dests[i].members[k]);
            if (!m || m->isclass) {
                fprintf(stderr, "lp: %s: class %s: %s is not a printer\n", file, dests[i].name,
                        dests[i].members[k]);
                exit(1);
            }
        }
}

/* ---- locks ---- */

/* open the lock file of printer d; -1 with a message */
static int lock_open(const struct dest *d)
{
    char path[PATH_MAX];
    if (snprintf(path, sizeof path, "%s/lp.%s.lock", lockdir, d->name) >= (int)sizeof path) {
        fprintf(stderr, "lp: %s: name too long\n", lockdir);
        return -1;
    }
    int fd = open(path, O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0666);
    if (fd < 0) {
        fprintf(stderr, "lp: %s: %s\n", path, strerror(errno));
        return -1;
    }
    struct stat st;
    if (fstat(fd, &st) == 0 && st.st_uid == geteuid())
        fchmod(fd, 0666);
    return fd;
}

static bool lock_fd(int fd, bool wait)
{
    struct flock fl;
    memset(&fl, 0, sizeof fl);
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    while (fcntl(fd, wait ? F_SETLKW : F_SETLK, &fl) != 0) {
        if (errno != EINTR)
            return false;
    }
    return true;
}

/* the next request number of the printer whose lock file is fd */
static unsigned long next_number(int fd)
{
    char buf[32];
    ssize_t n = pread(fd, buf, sizeof buf - 1, 0);
    unsigned long v = 0;
    if (n > 0) {
        buf[n] = '\0';
        v = strtoul(buf, NULL, 10);
    }
    v = v % 999999 + 1;
    int k = snprintf(buf, sizeof buf, "%lu\n", v);
    if (pwrite(fd, buf, (size_t)k, 0) != k || ftruncate(fd, k) != 0)
        fprintf(stderr, "lp: warning: the request number not kept: %s\n", strerror(errno));
    return v;
}

/* ---- the files ---- */

struct input {
    const char *name;           /* as given ("-": the standard input) */
    int fd;
};

/* the standard input copied to a file (once), for the copies */
static int stdin_copy(void)
{
    static int fd = -2;
    if (fd != -2)
        return fd < 0 ? -1 : dup(fd);
    char tmpl[PATH_MAX];
    const char *t = getenv("TMPDIR");
    snprintf(tmpl, sizeof tmpl, "%s/lpXXXXXX", t && *t ? t : "/tmp");
    fd = mkstemp(tmpl);
    if (fd < 0) {
        fprintf(stderr, "lp: %s: %s\n", tmpl, strerror(errno));
        fd = -1;
        return -1;
    }
    unlink(tmpl);
    char buf[65536];
    for (;;) {
        ssize_t n = read(0, buf, sizeof buf);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "lp: standard input: %s\n", strerror(errno));
            close(fd);
            fd = -1;
            return -1;
        }
        if (n == 0)
            break;
        if (write(fd, buf, (size_t)n) != n) {
            fprintf(stderr, "lp: %s: %s\n", tmpl, strerror(errno));
            close(fd);
            fd = -1;
            return -1;
        }
    }
    return dup(fd);
}

static bool put(int out, const char *s, size_t n)
{
    while (n > 0) {
        ssize_t k = write(out, s, n);
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

/* copy in (from its start) to out; false with a message */
static bool copy_file(const struct input *in, int out, const char *dname)
{
    if (lseek(in->fd, 0, SEEK_SET) < 0) {
        fprintf(stderr, "lp: %s: %s\n", in->name, strerror(errno));
        return false;
    }
    char buf[65536];
    for (;;) {
        ssize_t n = read(in->fd, buf, sizeof buf);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "lp: %s: %s\n", strcmp(in->name, "-") ? in->name : "standard input",
                    strerror(errno));
            return false;
        }
        if (n == 0)
            return true;
        if (!put(out, buf, (size_t)n)) {
            fprintf(stderr, "lp: %s: %s\n", dname, strerror(errno));
            return false;
        }
    }
}

/* ---- telling the user ---- */

static void mail_user(const char *user, const char *subject, const char *body)
{
    int p[2];
    if (pipe(p) != 0) {
        fprintf(stderr, "lp: pipe: %s\n", strerror(errno));
        status = 1;
        return;
    }
    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "lp: fork: %s\n", strerror(errno));
        status = 1;
        return;
    }
    if (pid == 0) {
        dup2(p[0], 0);
        close(p[0]);
        close(p[1]);
        execlp("mailx", "mailx", "-s", subject, user, (char *)NULL);
        fprintf(stderr, "lp: mailx: %s\n", strerror(errno));
        _exit(127);
    }
    close(p[0]);
    put(p[1], body, strlen(body));
    close(p[1]);
    int ws;
    while (waitpid(pid, &ws, 0) < 0 && errno == EINTR)
        ;
    if (!WIFEXITED(ws) || WEXITSTATUS(ws) != 0) {
        fprintf(stderr, "lp: mailx failed\n");
        status = 1;
    }
}

/* write msg on a terminal of user: false if there is none */
static bool tell_terminal(const char *user, const char *msg)
{
    if (!utx_open(NULL))
        return false;
    struct utx u;
    bool done = false;
    while (!done && utx_next(&u)) {
        if (u.type != USER_PROCESS || strcmp(u.user, user) != 0)
            continue;
        char *p = utx_ttypath(u.line);
        int fd = p ? open(p, O_WRONLY | O_NOCTTY | O_APPEND | O_NONBLOCK) : -1;
        free(p);
        if (fd < 0)
            continue;
        done = put(fd, msg, strlen(msg));
        close(fd);
    }
    utx_close();
    return done;
}

/* ---- the request ---- */

static void usage(void)
{
    fputs("usage: lp [-c] [-d dest] [-n copies] [-msw] [-o option]... [-t title] [file...]\n", stderr);
    exit(2);
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    const char *o_d = NULL, *o_t = NULL;
    bool o_m = false, o_s = false, o_w = false;
    long copies = 1;
    char **opts = xmalloc((size_t)argc * sizeof *opts);
    int nopts = 0;
    int c;
    while ((c = getopt(argc, argv, "cd:n:mo:st:w")) != -1) {
        switch (c) {
        case 'c': break;            /* always so */
        case 'd': o_d = optarg; break;
        case 'n': {
            char *end;
            errno = 0;
            copies = strtol(optarg, &end, 10);
            if (!isdigit((unsigned char)optarg[0]) || *end || errno || copies < 1 || copies > 1000000) {
                fprintf(stderr, "lp: invalid number of copies: %s\n", optarg);
                usage();
            }
            break;
        }
        case 'm': o_m = true; break;
        case 'o': opts[nopts++] = optarg; break;
        case 's': o_s = true; break;
        case 't': o_t = optarg; break;
        case 'w': o_w = true; break;
        default: usage();
        }
    }

    read_conf();
    const char *dname = o_d;
    if (!dname || !*dname)
        dname = getenv("LPDEST");
    if (!dname || !*dname)
        dname = getenv("PRINTER");
    if (!dname || !*dname)
        dname = default_dest;
    if ((!dname || !*dname) && ndests == 1)
        dname = dests[0].name;
    if (!dname || !*dname) {
        fputs("lp: no destination (no printer is configured)\n", stderr);
        return 1;
    }
    struct dest *dest = find_dest(dname);
    if (!dest) {
        fprintf(stderr, "lp: %s: no such destination\n", dname);
        return 1;
    }

    /* the options */
    bool banner = true, set_banner = false;
    char optstr[4096] = "";
    for (int i = 0; i < nopts; i++) {
        if (strcmp(opts[i], "nobanner") == 0) {
            banner = false;
            set_banner = true;
        } else if (strcmp(opts[i], "banner") == 0) {
            banner = true;
            set_banner = true;
        } else {
            size_t l = strlen(optstr);
            snprintf(optstr + l, sizeof optstr - l, "%s%s", l ? " " : "", opts[i]);
        }
    }

    /* the files */
    int nin = argc - optind > 0 ? argc - optind : 1;
    struct input *in = xmalloc((size_t)nin * sizeof *in);
    int nok = 0;
    for (int i = 0; i < nin; i++) {
        const char *f = argc - optind > 0 ? argv[optind + i] : "-";
        int fd;
        if (strcmp(f, "-") == 0) {
            fd = stdin_copy();
        } else {
            fd = open(f, O_RDONLY | O_CLOEXEC);
            struct stat st;
            if (fd < 0) {
                fprintf(stderr, "lp: %s: %s\n", f, strerror(errno));
            } else if (fstat(fd, &st) == 0 && S_ISDIR(st.st_mode)) {
                fprintf(stderr, "lp: %s: is a directory\n", f);
                close(fd);
                fd = -1;
            }
        }
        if (fd < 0) {
            status = 1;
            continue;
        }
        in[nok++] = (struct input){ f, fd };
    }
    if (nok == 0) {
        fputs("lp: nothing to print\n", stderr);
        return 1;
    }

    /* the printer, and exclusive access to it */
    struct dest *pr = dest;
    int lfd = -1;
    if (dest->isclass) {
        for (int k = 0; k < dest->nmembers && lfd < 0; k++) {
            struct dest *m = find_dest(dest->members[k]);
            int fd = lock_open(m);
            if (fd < 0)
                return 1;
            if (lock_fd(fd, false)) {
                pr = m;
                lfd = fd;
            } else {
                close(fd);
            }
        }
        if (lfd < 0) {
            pr = find_dest(dest->members[0]);
            if ((lfd = lock_open(pr)) < 0)
                return 1;
            if (!lock_fd(lfd, true)) {
                fprintf(stderr, "lp: %s: %s\n", pr->name, strerror(errno));
                return 1;
            }
        }
    } else {
        if ((lfd = lock_open(pr)) < 0)
            return 1;
        if (!lock_fd(lfd, true)) {
            fprintf(stderr, "lp: %s: %s\n", pr->name, strerror(errno));
            return 1;
        }
    }
    if (pr->device && optstr[0]) {
        fprintf(stderr, "lp: %s: options not supported: %s\n", pr->name, optstr);
        return 1;
    }
    if (!set_banner)
        banner = !pr->nobanner;

    char request[300];
    snprintf(request, sizeof request, "%s-%lu", pr->name, next_number(lfd));
    const char *user = getlogin();
    struct passwd *pw;
    if (!user || !*user || !(pw = getpwnam(user)) || pw->pw_uid != getuid()) {
        pw = getpwuid(getuid());
        user = pw ? pw->pw_name : "unknown";
    }
    user = xstrdup(user);

    /* the output */
    int out;
    pid_t child = -1;
    if (pr->device) {
        out = open(pr->device, O_WRONLY | O_APPEND | O_NOCTTY | O_CLOEXEC);
        if (out < 0) {
            fprintf(stderr, "lp: %s: %s: %s\n", pr->name, pr->device, strerror(errno));
            return 1;
        }
    } else {
        int p[2];
        char ncopies[32];
        snprintf(ncopies, sizeof ncopies, "%d", 1);
        if (pipe(p) != 0) {
            fprintf(stderr, "lp: pipe: %s\n", strerror(errno));
            return 1;
        }
        fflush(stdout);
        signal(SIGPIPE, SIG_IGN);
        child = fork();
        if (child < 0) {
            fprintf(stderr, "lp: fork: %s\n", strerror(errno));
            return 1;
        }
        if (child == 0) {
            signal(SIGPIPE, SIG_DFL);
            dup2(p[0], 0);
            close(p[0]);
            close(p[1]);
            setenv("LP_REQUEST", request, 1);
            setenv("LP_DEST", pr->name, 1);
            setenv("LP_USER", user, 1);
            setenv("LP_TITLE", o_t ? o_t : "", 1);
            setenv("LP_COPIES", ncopies, 1);
            setenv("LP_OPTIONS", optstr, 1);
            execlp("sh", "sh", "-c", pr->command, (char *)NULL);
            fprintf(stderr, "lp: sh: %s\n", strerror(errno));
            _exit(127);
        }
        close(p[0]);
        out = p[1];
    }

    bool ok = true;
    if (banner) {
        char date[256] = "";
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        if (tm)
            strftime(date, sizeof date, "%c", tm);
        size_t cap = 4096;
        for (int i = 0; i < nok; i++)
            cap += strlen(in[i].name) + 2;
        if (o_t)
            cap += strlen(o_t);
        char *b = xmalloc(cap);
        size_t k = (size_t)snprintf(b, cap, "\nRequest: %s\nUser:    %s\n", request, user);
        if (o_t)
            k += (size_t)snprintf(b + k, cap - k, "Title:   %s\n", o_t);
        k += (size_t)snprintf(b + k, cap - k, "Files:   ");
        for (int i = 0; i < nok; i++)
            k += (size_t)snprintf(b + k, cap - k, "%s%s", i ? ", " : "",
                                  strcmp(in[i].name, "-") ? in[i].name : "(standard input)");
        k += (size_t)snprintf(b + k, cap - k, "\nDate:    %s\n\f", date);
        if (!put(out, b, k)) {
            fprintf(stderr, "lp: %s: %s\n", pr->name, strerror(errno));
            ok = false;
        }
        free(b);
    }
    for (long n = 0; ok && n < copies; n++)
        for (int i = 0; ok && i < nok; i++)
            ok = copy_file(&in[i], out, pr->name);
    if (close(out) != 0 && ok) {
        fprintf(stderr, "lp: %s: %s\n", pr->name, strerror(errno));
        ok = false;
    }
    if (child > 0) {
        int ws;
        while (waitpid(child, &ws, 0) < 0 && errno == EINTR)
            ;
        if (ok && (!WIFEXITED(ws) || WEXITSTATUS(ws) != 0)) {
            fprintf(stderr, "lp: %s: the printer's command failed\n", pr->name);
            ok = false;
        }
    }
    close(lfd);                 /* the lock is released */
    if (!ok)
        return 1;

    if (!o_s) {
        printf("request id is %s (%d file%s)\n", request, nok, nok == 1 ? "" : "s");
        if (fflush(stdout) != 0) {
            fprintf(stderr, "lp: standard output: %s\n", strerror(errno));
            status = 1;
        }
    }
    char subject[400], body[800];
    snprintf(subject, sizeof subject, "lp: request %s", request);
    snprintf(body, sizeof body, "Your request %s%s%s%s has been printed on %s.\n", request,
             o_t ? " (" : "", o_t ? o_t : "", o_t ? ")" : "", pr->name);
    if (o_w) {
        char msg[800];
        snprintf(msg, sizeof msg, "\r\nlp: request %s has been printed\r\n", request);
        if (!tell_terminal(user, msg))
            mail_user(user, subject, body);
    }
    if (o_m)
        mail_user(user, subject, body);
    return status;
}
