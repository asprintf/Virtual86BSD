/*
 * uucomm.c - the UUCP system of uucp, uux, uustat and uucico
 *
 * The configuration: a file of lines "keyword arguments" (# comments):
 *     nodename NAME        this system's name (default: uname's nodename)
 *     spool DIR            the spool directory (/var/spool/uucp)
 *     pubdir DIR           the public directory (/var/spool/uucppublic)
 *     mail COMMAND         sends mail: run as COMMAND -s SUBJECT USER,
 *                          the message on its standard input (mailx)
 *     read DIR...          what other systems may read, besides pubdir
 *     write DIR...         what other systems may write, besides pubdir
 *     commands NAME...     what other systems may run (rmail rnews; ALL)
 *     system NAME COMMAND  a system, reached by a shell command whose
 *                          standard input and output are those of
 *                          "uucico -S" there (as "ssh NAME uucico -S")
 * read from $UUCP_CONF (unless the program is set-user-ID or
 * set-group-ID), or /etc/uucp/config.
 *
 * The conversation with another system: lines of words (in which the
 * bytes up to the space, "%" and those from 0x7f are written %XX), and
 * files as their size, then their bytes:
 *     HELLO node user              OK node
 *     PUT dest mode flags notify name size, data
 *                                  OK path | NO problem
 *     GET pattern                  OK n, then n FILE name mode size, data
 *     PUTX name mode size, data    OK
 *     RUN nwords insize, the words (W kind text sys path), the input
 *                                  OK status outsize errsize nout, the
 *                                  output, the error output, and for
 *                                  each output file "F size", data
 *                                  (or "F -")
 *     BYE
 * The same operations on this system are done directly; the files
 * travel in memory.
 *
 * The spool: jobs/ID (a job: lines "keyword words"), data/ (copies of
 * files, -C, and standard inputs, uux -p), status/SYSTEM (the last
 * attempt to reach it), exec/ (where commands run), SEQ (the job
 * numbers).  Its directories are made as needed, writable by everyone
 * with the sticky bit (the programs are not set-user-ID); a job's data
 * files are its user's only.
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include "uucomm.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

const char *progname = "uucp";
struct uuconf conf;

/* ---- memory ---- */

void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) {
        fprintf(stderr, "%s: out of memory\n", progname);
        exit(1);
    }
    return p;
}

void *xrealloc(void *p, size_t n)
{
    p = realloc(p, n ? n : 1);
    if (!p) {
        fprintf(stderr, "%s: out of memory\n", progname);
        exit(1);
    }
    return p;
}

char *xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    return memcpy(xmalloc(n), s, n);
}

char *xasprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *s = xmalloc((size_t)n + 1);
    va_start(ap, fmt);
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return s;
}

static void push(char ***v, size_t *n, const char *s)
{
    *v = xrealloc(*v, (*n + 1) * sizeof **v);
    (*v)[(*n)++] = xstrdup(s);
}

/* ---- the configuration ---- */

static const char *rest_of(const char *line, const char *word)
{
    const char *p = strstr(line, word) + strlen(word);
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

bool uu_config(void)
{
    struct utsname u;
    conf.nodename = xstrdup(uname(&u) == 0 ? u.nodename : "local");
    char *dot = strchr(conf.nodename, '.');
    if (dot)
        *dot = '\0';
    conf.spool = xstrdup("/var/spool/uucp");
    conf.pubdir = xstrdup("/var/spool/uucppublic");
    conf.mail = xstrdup("mailx");
    bool cmds_given = false;
    const char *path = NULL;
    if (getuid() == geteuid() && getgid() == getegid())
        path = getenv("UUCP_CONF");
    bool given = path && *path;
    if (!given)
        path = "/etc/uucp/config";
    FILE *f = fopen(path, "r");
    if (!f) {
        if (given || errno != ENOENT) {
            fprintf(stderr, "%s: %s: %s\n", progname, path, strerror(errno));
            return false;
        }
    } else {
        char *line = NULL;
        size_t cap = 0;
        ssize_t len;
        long lineno = 0;
        while ((len = getline(&line, &cap, f)) >= 0) {
            lineno++;
            if (len > 0 && line[len - 1] == '\n')
                line[--len] = '\0';
            char *hash = strchr(line, '#');
            if (hash)
                *hash = '\0';
            char *copy = xstrdup(line);
            char *save, *kw = strtok_r(copy, " \t", &save);
            if (!kw) {
                free(copy);
                continue;
            }
            char *arg = strtok_r(NULL, " \t", &save);
            if (strcmp(kw, "nodename") == 0 && arg) {
                free(conf.nodename);
                conf.nodename = xstrdup(arg);
            } else if (strcmp(kw, "spool") == 0 && arg) {
                free(conf.spool);
                conf.spool = xstrdup(arg);
            } else if (strcmp(kw, "pubdir") == 0 && arg) {
                free(conf.pubdir);
                conf.pubdir = xstrdup(arg);
            } else if (strcmp(kw, "mail") == 0 && arg) {
                free(conf.mail);
                conf.mail = xstrdup(rest_of(line, "mail"));
            } else if (strcmp(kw, "read") == 0 || strcmp(kw, "write") == 0) {
                for (; arg; arg = strtok_r(NULL, " \t", &save)) {
                    if (kw[0] == 'r')
                        push(&conf.read, &conf.nread, arg);
                    else
                        push(&conf.write, &conf.nwrite, arg);
                }
            } else if (strcmp(kw, "commands") == 0) {
                cmds_given = true;
                for (; arg; arg = strtok_r(NULL, " \t", &save))
                    push(&conf.commands, &conf.ncommands, arg);
            } else if (strcmp(kw, "system") == 0 && arg && uu_sysname(arg)) {
                const char *cmd = rest_of(rest_of(line, "system"), arg);
                conf.systems = xrealloc(conf.systems, (conf.nsystems + 1) * sizeof *conf.systems);
                conf.systems[conf.nsystems].name = xstrdup(arg);
                conf.systems[conf.nsystems].command = xstrdup(cmd);
                conf.nsystems++;
            } else {
                fprintf(stderr, "%s: %s:%ld: bad line\n", progname, path, lineno);
                free(copy);
                fclose(f);
                free(line);
                return false;
            }
            free(copy);
        }
        free(line);
        fclose(f);
    }
    if (!cmds_given) {
        push(&conf.commands, &conf.ncommands, "rmail");
        push(&conf.commands, &conf.ncommands, "rnews");
    }
    return true;
}

bool uu_is_local(const char *name)
{
    return !name || !*name || strcmp(name, conf.nodename) == 0;
}

static const struct uusystem *find_system(const char *name)
{
    for (size_t i = 0; i < conf.nsystems; i++)
        if (strcmp(conf.systems[i].name, name) == 0)
            return &conf.systems[i];
    return NULL;
}

bool uu_known(const char *name)
{
    return uu_is_local(name) || find_system(name);
}

const char *uu_user(void)
{
    static char *name;
    if (!name) {
        struct passwd *pw = getpwuid(getuid());
        name = pw ? xstrdup(pw->pw_name) : xasprintf("%ld", (long)getuid());
    }
    return name;
}

/* ---- paths ---- */

bool uu_sysname(const char *s)
{
    if (!*s)
        return false;
    for (; *s; s++)
        if (!isalnum((unsigned char)*s) && *s != '-' && *s != '_' && *s != '.')
            return false;
    return true;
}

void uu_split(const char *arg, char **sys, char **path)
{
    const char *bang = strchr(arg, '!');
    if (!bang) {
        *sys = NULL;
        *path = xstrdup(arg);
        return;
    }
    size_t n = (size_t)(bang - arg);
    *sys = xmalloc(n + 1);
    memcpy(*sys, arg, n);
    (*sys)[n] = '\0';
    *path = xstrdup(bang + 1);
}

char *uu_absolute(const char *path)
{
    if (path[0] == '/' || path[0] == '~')
        return xstrdup(path);
    char *cwd = getcwd(NULL, 0);
    if (!cwd)
        return xstrdup(path);
    char *r = xasprintf("%s/%s", strcmp(cwd, "/") == 0 ? "" : cwd, path);
    free(cwd);
    return r;
}

char *uu_expand(const char *path)
{
    if (path[0] == '~') {
        const char *slash = strchr(path, '/');
        size_t n = slash ? (size_t)(slash - path - 1) : strlen(path + 1);
        const char *rest = slash ? slash : "";
        if (n == 0)
            return xasprintf("%s%s", conf.pubdir, rest);
        char *user = xmalloc(n + 1);
        memcpy(user, path + 1, n);
        user[n] = '\0';
        struct passwd *pw = getpwnam(user);
        free(user);
        /* an unknown login: the public directory */
        return xasprintf("%s%s", pw && pw->pw_dir ? pw->pw_dir : conf.pubdir, rest);
    }
    if (path[0] != '/')
        return xasprintf("%s/%s", conf.pubdir, path);
    return xstrdup(path);
}

static bool under(const char *path, const char *dir)
{
    size_t n = strlen(dir);
    while (n > 1 && dir[n - 1] == '/')
        n--;
    return strncmp(path, dir, n) == 0 && (path[n] == '/' || path[n] == '\0');
}

bool uu_allowed(const char *path, bool write)
{
    if (path[0] != '/')
        return false;
    /* no ".." component */
    for (const char *p = path; (p = strstr(p, "..")) != NULL; p += 2)
        if (p[-1] == '/' && (p[2] == '/' || p[2] == '\0'))
            return false;
    if (under(path, conf.pubdir))
        return true;
    for (size_t i = 0; i < conf.nwrite; i++)
        if (under(path, conf.write[i]))
            return true;
    if (!write)
        for (size_t i = 0; i < conf.nread; i++)
            if (under(path, conf.read[i]))
                return true;
    return false;
}

char *uu_slurp(const char *path, size_t *size)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return NULL;
    size_t cap = 8192, n = 0;
    char *d = xmalloc(cap);
    for (;;) {
        if (n == cap) {
            cap *= 2;
            d = xrealloc(d, cap);
        }
        ssize_t r = read(fd, d + n, cap - n);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            int e = errno;
            close(fd);
            free(d);
            errno = e;
            return NULL;
        }
        if (r == 0)
            break;
        n += (size_t)r;
    }
    close(fd);
    *size = n;
    return d;
}

static bool write_all(int fd, const char *d, size_t n)
{
    while (n > 0) {
        ssize_t w = write(fd, d, n);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        d += w;
        n -= (size_t)w;
    }
    return true;
}

static bool mkdirs(const char *path, mode_t mode)
{
    char *p = xstrdup(path);
    for (char *s = p + 1; *s; s++) {
        if (*s != '/')
            continue;
        *s = '\0';
        if (mkdir(p, mode) != 0 && errno != EEXIST) {
            free(p);
            return false;
        }
        *s = '/';
    }
    bool ok = mkdir(p, mode) == 0 || errno == EEXIST;
    free(p);
    return ok;
}

static const char *base_name(const char *path)
{
    const char *s = strrchr(path, '/');
    return s ? s + 1 : path;
}

void uu_free_files(struct uufile *f, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        free(f[i].name);
        free(f[i].data);
    }
    free(f);
}

/* ---- the operations, on this system ---- */

struct session {
    bool remote;                /* for another system: restricted */
    char *peer, *peer_user;
    char *xdir;                 /* where commands run */
};

static char *exec_dir(struct session *s)
{
    if (s->xdir)
        return s->xdir;
    char *base = xasprintf("%s/exec", conf.spool);
    mkdirs(base, 0755);
    chmod(base, 01777);
    char *tmpl = xasprintf("%s/XXXXXX", base);
    free(base);
    if (!mkdtemp(tmpl)) {
        free(tmpl);
        return NULL;
    }
    s->xdir = tmpl;
    return s->xdir;
}

static void end_session(struct session *s)
{
    if (s->xdir) {
        DIR *d = opendir(s->xdir);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d)) != NULL) {
                if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
                    continue;
                char *p = xasprintf("%s/%s", s->xdir, e->d_name);
                unlink(p);
                free(p);
            }
            closedir(d);
        }
        rmdir(s->xdir);
        free(s->xdir);
        s->xdir = NULL;
    }
}

static bool s_put(struct session *s, const char *dest, mode_t mode, const char *flags,
                  const char *notify, const struct uufile *f, char **final, char **err);

/* a file sent on along a route: a job of this system */
static bool forward(struct session *s, const char *sys, const char *path, const char *flags,
                    const char *notify, const struct uufile *f, char **final, char **err)
{
    if (!uu_known(sys)) {
        *err = xasprintf("unknown system %s", sys);
        return false;
    }
    if (uu_is_local(sys))
        return s_put(s, path, f->mode, flags, notify, f, final, err);
    struct uujob j = { 0 };
    j.id = uu_new_id(sys);
    if (!j.id) {
        *err = xasprintf("cannot queue for %s", sys);
        return false;
    }
    j.user = (char *)uu_user();
    j.system = (char *)sys;
    j.time = time(NULL);
    j.flags = (char *)flags;
    j.notify = (char *)notify;
    struct uuitem it = { NULL, NULL, (char *)sys, (char *)path, NULL };
    it.data = uu_spool_data(j.id, 0, f->data, f->size);
    it.spath = (char *)f->name;
    if (!it.data) {
        *err = xasprintf("cannot spool %s", f->name);
        free(j.id);
        return false;
    }
    j.items = &it;
    j.nitems = 1;
    if (!uu_save(&j)) {
        *err = xasprintf("cannot queue for %s", sys);
        return false;
    }
    struct uujob *k = uu_load(j.id);
    bool temp = false;
    char *e = NULL;
    if (k && uu_run_job(k, &temp, &e)) {
        *final = xasprintf("%s!%s", sys, path);
    } else if (k && temp) {
        *final = xasprintf("%s!%s (queued: %s)", sys, path, e ? e : "");
    } else {
        *err = e ? e : xasprintf("cannot send to %s", sys);
        uu_free_job(k);
        free(j.id);
        free(it.data);
        return false;
    }
    free(e);
    uu_free_job(k);
    free(j.id);
    free(it.data);
    return true;
}

static bool s_put(struct session *s, const char *dest, mode_t mode, const char *flags,
                  const char *notify, const struct uufile *f, char **final, char **err)
{
    char *sys, *rest;
    uu_split(dest, &sys, &rest);
    if (sys && uu_sysname(sys)) {
        bool r = forward(s, sys, rest, flags, notify, f, final, err);
        free(sys);
        free(rest);
        return r;
    }
    free(sys);
    free(rest);
    char *path = uu_expand(dest);
    if (s->remote && !uu_allowed(path, true)) {
        *err = xasprintf("%s: not permitted", path);
        free(path);
        return false;
    }
    bool mkd = !strchr(flags, 'f');
    size_t len = strlen(path);
    bool isdir = strchr(flags, 'D') || (len > 0 && path[len - 1] == '/');
    struct stat st;
    if (!isdir && stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        isdir = true;
    if (isdir) {
        while (len > 1 && path[len - 1] == '/')
            path[--len] = '\0';
        if (stat(path, &st) != 0 && (!mkd || !mkdirs(path, 0755))) {
            *err = xasprintf("%s: %s", path, mkd ? strerror(errno) : "no such directory");
            free(path);
            return false;
        }
        char *p = xasprintf("%s/%s", path, base_name(f->name));
        free(path);
        path = p;
    } else if (mkd) {
        char *dir = xstrdup(path);
        char *slash = strrchr(dir, '/');
        if (slash && slash != dir) {
            *slash = '\0';
            if (!mkdirs(dir, 0755)) {
                *err = xasprintf("%s: %s", dir, strerror(errno));
                free(dir);
                free(path);
                return false;
            }
        }
        free(dir);
    }
    if (s->remote && !uu_allowed(path, true)) {
        *err = xasprintf("%s: not permitted", path);
        free(path);
        return false;
    }
    /* a new file beside it, renamed over it */
    char *tmp = xasprintf("%s.uuXXXXXX", path);
    char *slash = strrchr(tmp, '/');
    (void)slash;
    int fd = mkstemp(tmp);
    if (fd < 0) {
        *err = xasprintf("%s: %s", path, strerror(errno));
        free(tmp);
        free(path);
        return false;
    }
    mode_t m = (mode & 0111) ? 0777 : 0666;
    mode_t um = umask(0);
    umask(um);
    if (!write_all(fd, f->data, f->size) || fchmod(fd, m & ~um) != 0 || close(fd) != 0 ||
        rename(tmp, path) != 0) {
        *err = xasprintf("%s: %s", path, strerror(errno));
        unlink(tmp);
        free(tmp);
        free(path);
        return false;
    }
    free(tmp);
    if (notify && *notify) {
        char *body = xasprintf("%s!%s sent %s, now %s.\n", s->peer ? s->peer : conf.nodename,
                               s->peer_user ? s->peer_user : uu_user(), f->name, path);
        uu_mail(notify, "uucp: a file arrived", body);
        free(body);
    }
    *final = path;
    (void)mode;
    return true;
}

static bool add_file(struct uufile **v, size_t *n, const char *path, char **err)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        *err = xasprintf("%s: %s", path, strerror(errno));
        return false;
    }
    if (!S_ISREG(st.st_mode)) {
        *err = xasprintf("%s: not a regular file", path);
        return false;
    }
    size_t size;
    char *d = uu_slurp(path, &size);
    if (!d) {
        *err = xasprintf("%s: %s", path, strerror(errno));
        return false;
    }
    *v = xrealloc(*v, (*n + 1) * sizeof **v);
    (*v)[*n] = (struct uufile){ xstrdup(path), st.st_mode & 07777, d, size };
    (*n)++;
    return true;
}

static bool s_get(struct session *s, const char *pattern, struct uufile **files, size_t *n, char **err)
{
    char *path = uu_expand(pattern);
    *files = NULL;
    *n = 0;
    glob_t g;
    int r = glob(path, GLOB_NOCHECK, NULL, &g);
    if (r != 0) {
        *err = xasprintf("%s: no such file", path);
        free(path);
        return false;
    }
    for (size_t i = 0; i < g.gl_pathc; i++) {
        if (s->remote && !uu_allowed(g.gl_pathv[i], false)) {
            *err = xasprintf("%s: not permitted", g.gl_pathv[i]);
            goto fail;
        }
        if (!add_file(files, n, g.gl_pathv[i], err))
            goto fail;
    }
    globfree(&g);
    free(path);
    return true;
fail:
    globfree(&g);
    free(path);
    uu_free_files(*files, *n);
    *files = NULL;
    *n = 0;
    return false;
}

static bool simple_name(const char *name)
{
    return *name && !strchr(name, '/') && strcmp(name, ".") != 0 && strcmp(name, "..") != 0 &&
           strncmp(name, ".uux.", 5) != 0;
}

static bool s_putx(struct session *s, const char *name, const struct uufile *f, char **err)
{
    if (!simple_name(name)) {
        *err = xasprintf("%s: not a file name", name);
        return false;
    }
    char *dir = exec_dir(s);
    if (!dir) {
        *err = xasprintf("cannot make a directory to run in: %s", strerror(errno));
        return false;
    }
    char *path = xasprintf("%s/%s", dir, name);
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, (f->mode & 0111) ? 0700 : 0600);
    if (fd < 0) {
        *err = errno == EEXIST ? xasprintf("%s: two files of the same name", name)
                               : xasprintf("%s: %s", name, strerror(errno));
        free(path);
        return false;
    }
    bool ok = write_all(fd, f->data, f->size);
    if (close(fd) != 0)
        ok = false;
    if (!ok)
        *err = xasprintf("%s: %s", name, strerror(errno));
    free(path);
    return ok;
}

static char *shell_quote(const char *s)
{
    size_t n = 3;
    for (const char *p = s; *p; p++)
        n += *p == '\'' ? 4 : 1;
    char *r = xmalloc(n), *q = r;
    *q++ = '\'';
    for (; *s; s++) {
        if (*s == '\'') {
            memcpy(q, "'\\''", 4);
            q += 4;
        } else {
            *q++ = *s;
        }
    }
    *q++ = '\'';
    *q = '\0';
    return r;
}

static bool command_allowed(const char *name)
{
    for (size_t i = 0; i < conf.ncommands; i++)
        if (strcmp(conf.commands[i], "ALL") == 0 || strcmp(conf.commands[i], name) == 0)
            return true;
    return false;
}

static bool s_run(struct session *s, const struct uucmd *cmd, const struct uufile *in, int *status,
                  struct uufile *out, struct uufile *errout, struct uufile **outfiles, char **err)
{
    size_t nout = 0;
    for (size_t i = 0; i < cmd->n; i++)
        if (cmd->w[i].kind == W_OUT)
            nout++;
    *outfiles = NULL;
    /* the command line: every word quoted */
    size_t cap = 256, len = 0;
    char *line = xmalloc(cap);
    line[0] = '\0';
    bool at_start = true;
    for (size_t i = 0; i < cmd->n; i++) {
        const struct uuword *w = &cmd->w[i];
        char *word;
        if (w->kind == W_OP) {
            if (strcmp(w->text, "|") != 0 && strcmp(w->text, "<") != 0 && strcmp(w->text, ">") != 0) {
                *err = xasprintf("%s: not allowed", w->text);
                free(line);
                return false;
            }
            word = xstrdup(w->text);
        } else if (w->kind == W_PATH) {
            char *p = uu_expand(w->text);
            bool output = i > 0 && cmd->w[i - 1].kind == W_OP && strcmp(cmd->w[i - 1].text, ">") == 0;
            if (s->remote && !uu_allowed(p, output)) {
                *err = xasprintf("%s: not permitted", p);
                free(p);
                free(line);
                return false;
            }
            word = shell_quote(p);
            free(p);
        } else {
            if (at_start && w->kind == W_LIT && s->remote && !command_allowed(w->text)) {
                *err = xasprintf("%s: command not permitted", w->text);
                free(line);
                return false;
            }
            if (w->kind != W_LIT && !simple_name(w->text)) {
                *err = xasprintf("%s: not a file name", w->text);
                free(line);
                return false;
            }
            word = shell_quote(w->text);
        }
        at_start = w->kind == W_OP && strcmp(w->text, "|") == 0;
        size_t l = strlen(word);
        if (len + l + 2 > cap) {
            cap = (len + l + 2) * 2;
            line = xrealloc(line, cap);
        }
        if (len)
            line[len++] = ' ';
        memcpy(line + len, word, l + 1);
        len += l;
        free(word);
    }
    char *dir = exec_dir(s);
    if (!dir) {
        *err = xasprintf("cannot make a directory to run in: %s", strerror(errno));
        free(line);
        return false;
    }
    char *inpath = xasprintf("%s/.uux.in", dir), *outpath = xasprintf("%s/.uux.out", dir);
    char *errpath = xasprintf("%s/.uux.err", dir);
    int fd = open(inpath, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0 || !write_all(fd, in ? in->data : "", in ? in->size : 0) || close(fd) != 0) {
        *err = xasprintf("%s: %s", inpath, strerror(errno));
        free(line);
        free(inpath);
        free(outpath);
        free(errpath);
        return false;
    }
    fflush(NULL);
    pid_t pid = fork();
    if (pid < 0) {
        *err = xasprintf("fork: %s", strerror(errno));
        free(line);
        free(inpath);
        free(outpath);
        free(errpath);
        return false;
    }
    if (pid == 0) {
        int i0 = open(inpath, O_RDONLY), o = open(outpath, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        int e = s->remote ? open(errpath, O_WRONLY | O_CREAT | O_TRUNC, 0600) : 2;
        if (i0 < 0 || o < 0 || e < 0 || dup2(i0, 0) < 0 || dup2(o, 1) < 0 || dup2(e, 2) < 0 ||
            chdir(dir) != 0)
            _exit(126);
        signal(SIGPIPE, SIG_DFL);
        execl("/bin/sh", "sh", "-c", line, (char *)NULL);
        _exit(127);
    }
    int st;
    while (waitpid(pid, &st, 0) < 0)
        if (errno != EINTR)
            break;
    *status = WIFEXITED(st) ? WEXITSTATUS(st) : WIFSIGNALED(st) ? 128 + WTERMSIG(st) : 1;
    free(line);
    size_t size = 0;
    out->name = xstrdup("standard output");
    out->mode = 0644;
    out->data = uu_slurp(outpath, &size);
    out->size = out->data ? size : 0;
    if (!out->data)
        out->data = xstrdup("");
    errout->name = xstrdup("standard error");
    errout->mode = 0644;
    errout->data = s->remote ? uu_slurp(errpath, &size) : NULL;
    errout->size = errout->data ? size : 0;
    if (!errout->data)
        errout->data = xstrdup("");
    unlink(inpath);
    unlink(outpath);
    unlink(errpath);
    free(inpath);
    free(outpath);
    free(errpath);
    if (nout) {
        *outfiles = xmalloc(nout * sizeof **outfiles);
        size_t k = 0;
        for (size_t i = 0; i < cmd->n; i++) {
            if (cmd->w[i].kind != W_OUT)
                continue;
            char *p = xasprintf("%s/%s", dir, cmd->w[i].text);
            size_t sz = 0;
            char *d = uu_slurp(p, &sz);
            (*outfiles)[k++] = (struct uufile){ xstrdup(cmd->w[i].text), 0644, d, d ? sz : 0 };
            free(p);
        }
    }
    return true;
}

/* ---- the conversation ---- */

struct conn {
    bool local;
    char *name;
    struct session sess;        /* local */
    pid_t pid;
    FILE *to, *from;
};

static char *enc(const char *s)
{
    if (!s)
        return xstrdup("-");
    if (!*s)
        return xstrdup("%");
    size_t n = 1;
    for (const char *p = s; *p; p++)
        n += ((unsigned char)*p <= ' ' || *p == '%' || (unsigned char)*p >= 0x7f || (p == s && *p == '-')) ? 3 : 1;
    char *r = xmalloc(n), *q = r;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c <= ' ' || c == '%' || c >= 0x7f || (p == s && c == '-'))
            q += sprintf(q, "%%%02X", c);
        else
            *q++ = (char)c;
    }
    *q = '\0';
    return r;
}

/* decode in place; "-" is NULL, "%" is "" */
static char *dec(char *s)
{
    if (strcmp(s, "-") == 0)
        return NULL;
    if (strcmp(s, "%") == 0) {
        s[0] = '\0';
        return s;
    }
    char *q = s;
    for (char *p = s; *p; p++) {
        if (*p == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
            char h[3] = { p[1], p[2], 0 };
            *q++ = (char)strtol(h, NULL, 16);
            p += 2;
        } else {
            *q++ = *p;
        }
    }
    *q = '\0';
    return s;
}

/* read a line; split into words (at most max); the words point into *buf */
static int read_words(FILE *f, char **buf, char **w, int max)
{
    size_t cap = 0;
    free(*buf);
    *buf = NULL;
    ssize_t len = getline(buf, &cap, f);
    if (len <= 0)
        return -1;
    if ((*buf)[len - 1] == '\n')
        (*buf)[--len] = '\0';
    int n = 0;
    char *save, *t = strtok_r(*buf, " ", &save);
    while (t && n < max) {
        w[n++] = t;
        t = strtok_r(NULL, " ", &save);
    }
    return n;
}

static bool read_data(FILE *f, size_t size, char **data)
{
    *data = xmalloc(size + 1);
    if (size && fread(*data, 1, size, f) != size) {
        free(*data);
        *data = NULL;
        return false;
    }
    (*data)[size] = '\0';
    return true;
}

static bool parse_size(const char *s, size_t *v)
{
    char *end;
    errno = 0;
    unsigned long long n = strtoull(s, &end, 10);
    if (!*s || *end || errno)
        return false;
    *v = (size_t)n;
    return true;
}

static void send_words(FILE *f, int n, ...)
{
    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        const char *s = va_arg(ap, const char *);
        char *e = enc(s);
        fprintf(f, "%s%s", i ? " " : "", e);
        free(e);
    }
    va_end(ap);
    putc('\n', f);
}

static bool reply_ok(struct conn *c, char **buf, char **w, int max, int *n, char **err)
{
    *n = read_words(c->from, buf, w, max);
    if (*n < 1) {
        *err = xasprintf("%s: the connection was lost", c->name);
        return false;
    }
    if (strcmp(w[0], "OK") == 0)
        return true;
    if (strcmp(w[0], "NO") == 0 && *n >= 2) {
        char *m = dec(w[1]);
        *err = xasprintf("%s: %s", c->name, m ? m : "refused");
    } else {
        *err = xasprintf("%s: a bad answer", c->name);
    }
    return false;
}

struct conn *uu_connect(const char *sys, char **err)
{
    struct conn *c = calloc(1, sizeof *c);
    if (!c) {
        *err = xstrdup("out of memory");
        return NULL;
    }
    if (uu_is_local(sys)) {
        c->local = true;
        c->name = xstrdup(conf.nodename);
        return c;
    }
    c->name = xstrdup(sys);
    const struct uusystem *s = find_system(sys);
    if (!s) {
        *err = xasprintf("unknown system %s", sys);
        free(c->name);
        free(c);
        return NULL;
    }
    int to[2], from[2];
    if (pipe(to) != 0 || pipe(from) != 0) {
        *err = xasprintf("pipe: %s", strerror(errno));
        free(c->name);
        free(c);
        return NULL;
    }
    signal(SIGPIPE, SIG_IGN);
    fflush(NULL);
    c->pid = fork();
    if (c->pid < 0) {
        *err = xasprintf("fork: %s", strerror(errno));
        free(c->name);
        free(c);
        return NULL;
    }
    if (c->pid == 0) {
        dup2(to[0], 0);
        dup2(from[1], 1);
        close(to[0]);
        close(to[1]);
        close(from[0]);
        close(from[1]);
        signal(SIGPIPE, SIG_DFL);
        execl("/bin/sh", "sh", "-c", s->command, (char *)NULL);
        _exit(127);
    }
    close(to[0]);
    close(from[1]);
    c->to = fdopen(to[1], "w");
    c->from = fdopen(from[0], "r");
    send_words(c->to, 3, "HELLO", conf.nodename, uu_user());
    fflush(c->to);
    char *buf = NULL, *w[4];
    int n;
    if (!reply_ok(c, &buf, w, 4, &n, err)) {
        char *e = *err;
        size_t l = strlen(c->name);
        *err = xasprintf("cannot reach %s: %s", sys, strncmp(e, c->name, l) == 0 && e[l] == ':' ? e + l + 2 : e);
        free(e);
        free(buf);
        uu_disconnect(c);
        uu_status(sys, *err);
        return NULL;
    }
    free(buf);
    uu_status(sys, "reached");
    return c;
}

void uu_disconnect(struct conn *c)
{
    if (!c)
        return;
    if (c->local) {
        end_session(&c->sess);
    } else {
        if (c->to) {
            fputs("BYE\n", c->to);
            fclose(c->to);
        }
        if (c->from)
            fclose(c->from);
        int st;
        while (waitpid(c->pid, &st, 0) < 0 && errno == EINTR)
            ;
    }
    free(c->name);
    free(c);
}

static bool flush_ok(struct conn *c, char **err)
{
    if (fflush(c->to) != 0 || ferror(c->to)) {
        *err = xasprintf("%s: the connection was lost", c->name);
        return false;
    }
    return true;
}

bool uu_put(struct conn *c, const char *dest, mode_t mode, const char *flags, const char *notify,
            const struct uufile *f, char **final, char **err)
{
    if (c->local)
        return s_put(&c->sess, dest, mode, flags, notify, f, final, err);
    char m[16], sz[32];
    snprintf(m, sizeof m, "%o", (unsigned)(mode & 07777));
    snprintf(sz, sizeof sz, "%zu", f->size);
    send_words(c->to, 7, "PUT", dest, m, flags, notify, f->name, sz);
    fwrite(f->data, 1, f->size, c->to);
    if (!flush_ok(c, err))
        return false;
    char *buf = NULL, *w[3];
    int n;
    bool ok = reply_ok(c, &buf, w, 3, &n, err);
    if (ok) {
        char *p = n >= 2 ? dec(w[1]) : NULL;
        *final = xstrdup(p ? p : dest);
    }
    free(buf);
    return ok;
}

bool uu_get(struct conn *c, const char *pattern, struct uufile **files, size_t *n, char **err)
{
    if (c->local)
        return s_get(&c->sess, pattern, files, n, err);
    send_words(c->to, 2, "GET", pattern);
    if (!flush_ok(c, err))
        return false;
    char *buf = NULL, *w[5];
    int k;
    *files = NULL;
    *n = 0;
    if (!reply_ok(c, &buf, w, 5, &k, err)) {
        free(buf);
        return false;
    }
    size_t count;
    if (k < 2 || !parse_size(w[1], &count)) {
        *err = xasprintf("%s: a bad answer", c->name);
        free(buf);
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        size_t size;
        k = read_words(c->from, &buf, w, 5);
        if (k < 4 || strcmp(w[0], "FILE") != 0 || !parse_size(w[3], &size)) {
            *err = xasprintf("%s: a bad answer", c->name);
            break;
        }
        char *name = dec(w[1]);
        mode_t mode = (mode_t)strtoul(w[2], NULL, 8);
        char *data;
        name = xstrdup(name ? name : "");
        if (!read_data(c->from, size, &data)) {
            *err = xasprintf("%s: the connection was lost", c->name);
            free(name);
            break;
        }
        *files = xrealloc(*files, (*n + 1) * sizeof **files);
        (*files)[(*n)++] = (struct uufile){ name, mode, data, size };
    }
    free(buf);
    if (*n != count) {
        uu_free_files(*files, *n);
        *files = NULL;
        *n = 0;
        return false;
    }
    return true;
}

bool uu_putx(struct conn *c, const char *name, const struct uufile *f, char **err)
{
    if (c->local)
        return s_putx(&c->sess, name, f, err);
    char m[16], sz[32];
    snprintf(m, sizeof m, "%o", (unsigned)(f->mode & 07777));
    snprintf(sz, sizeof sz, "%zu", f->size);
    send_words(c->to, 4, "PUTX", name, m, sz);
    fwrite(f->data, 1, f->size, c->to);
    if (!flush_ok(c, err))
        return false;
    char *buf = NULL, *w[3];
    int n;
    bool ok = reply_ok(c, &buf, w, 3, &n, err);
    free(buf);
    return ok;
}

static const char *kind_name(int k)
{
    static const char *names[] = { "lit", "path", "gather", "out", "op" };
    return k >= 0 && k <= W_OP ? names[k] : "lit";
}

static int kind_of(const char *s)
{
    static const char *names[] = { "lit", "path", "gather", "out", "op" };
    for (int i = 0; i <= W_OP; i++)
        if (s && strcmp(s, names[i]) == 0)
            return i;
    return -1;
}

bool uu_run(struct conn *c, const struct uucmd *cmd, const struct uufile *in, int *status,
            struct uufile *out, struct uufile **outfiles, char **err)
{
    if (c->local) {
        struct uufile e = { 0 };
        bool ok = s_run(&c->sess, cmd, in, status, out, &e, outfiles, err);
        free(e.name);
        free(e.data);
        return ok;
    }
    char nw[32], sz[32];
    snprintf(nw, sizeof nw, "%zu", cmd->n);
    snprintf(sz, sizeof sz, "%zu", in ? in->size : 0);
    send_words(c->to, 3, "RUN", nw, sz);
    for (size_t i = 0; i < cmd->n; i++)
        send_words(c->to, 5, "W", kind_name(cmd->w[i].kind), cmd->w[i].text, cmd->w[i].sys, cmd->w[i].path);
    if (in)
        fwrite(in->data, 1, in->size, c->to);
    if (!flush_ok(c, err))
        return false;
    char *buf = NULL, *w[6];
    int n;
    *outfiles = NULL;
    if (!reply_ok(c, &buf, w, 6, &n, err)) {
        free(buf);
        return false;
    }
    size_t osz, esz, nout;
    if (n < 5 || !parse_size(w[2], &osz) || !parse_size(w[3], &esz) || !parse_size(w[4], &nout)) {
        *err = xasprintf("%s: a bad answer", c->name);
        free(buf);
        return false;
    }
    *status = atoi(w[1]);
    char *od, *ed;
    if (!read_data(c->from, osz, &od) || !read_data(c->from, esz, &ed)) {
        *err = xasprintf("%s: the connection was lost", c->name);
        free(buf);
        return false;
    }
    /* the remote command's error output, here */
    fwrite(ed, 1, esz, stderr);
    free(ed);
    *out = (struct uufile){ xstrdup("standard output"), 0644, od, osz };
    if (nout)
        *outfiles = calloc(nout, sizeof **outfiles);
    for (size_t i = 0; i < nout; i++) {
        int k = read_words(c->from, &buf, w, 3);
        size_t size;
        if (k < 2 || strcmp(w[0], "F") != 0) {
            *err = xasprintf("%s: a bad answer", c->name);
            free(buf);
            return false;
        }
        if (strcmp(w[1], "-") == 0)
            continue;
        if (!parse_size(w[1], &size) || !read_data(c->from, size, &(*outfiles)[i].data)) {
            *err = xasprintf("%s: the connection was lost", c->name);
            free(buf);
            return false;
        }
        (*outfiles)[i].size = size;
        (*outfiles)[i].mode = 0644;
    }
    free(buf);
    return true;
}

/* ---- the other side: uucico -S ---- */

static void answer_no(const char *msg)
{
    send_words(stdout, 2, "NO", msg);
}

int uu_slave(void)
{
    struct session s = { true, NULL, NULL, NULL };
    char *buf = NULL, *w[8];
    int n = read_words(stdin, &buf, w, 8);
    if (n < 3 || strcmp(w[0], "HELLO") != 0) {
        answer_no("expected HELLO");
        fflush(stdout);
        return 1;
    }
    s.peer = xstrdup(dec(w[1]) ? w[1] : "?");
    s.peer_user = xstrdup(dec(w[2]) ? w[2] : "?");
    send_words(stdout, 2, "OK", conf.nodename);
    fflush(stdout);
    while ((n = read_words(stdin, &buf, w, 8)) >= 1) {
        char *err = NULL;
        if (strcmp(w[0], "BYE") == 0)
            break;
        if (strcmp(w[0], "PUT") == 0 && n == 7) {
            size_t size;
            struct uufile f = { 0 };
            if (!parse_size(w[6], &size) || !read_data(stdin, size, &f.data))
                break;
            char *dest = dec(w[1]), *flags = dec(w[3]), *notify = dec(w[4]), *name = dec(w[5]);
            f.name = name ? name : "file";
            f.mode = (mode_t)strtoul(w[2], NULL, 8);
            f.size = size;
            char *final = NULL;
            if (dest && s_put(&s, dest, f.mode, flags ? flags : "", notify, &f, &final, &err))
                send_words(stdout, 2, "OK", final);
            else
                answer_no(err ? err : "bad request");
            free(final);
            free(f.data);
        } else if (strcmp(w[0], "GET") == 0 && n == 2) {
            struct uufile *files;
            size_t k;
            char *pattern = dec(w[1]);
            if (pattern && s_get(&s, pattern, &files, &k, &err)) {
                printf("OK %zu\n", k);
                for (size_t i = 0; i < k; i++) {
                    char m[16], sz[32];
                    snprintf(m, sizeof m, "%o", (unsigned)files[i].mode);
                    snprintf(sz, sizeof sz, "%zu", files[i].size);
                    send_words(stdout, 4, "FILE", files[i].name, m, sz);
                    fwrite(files[i].data, 1, files[i].size, stdout);
                }
                uu_free_files(files, k);
            } else {
                answer_no(err ? err : "bad request");
            }
        } else if (strcmp(w[0], "PUTX") == 0 && n == 4) {
            size_t size;
            struct uufile f = { 0 };
            if (!parse_size(w[3], &size) || !read_data(stdin, size, &f.data))
                break;
            char *name = dec(w[1]);
            f.name = name ? name : "";
            f.mode = (mode_t)strtoul(w[2], NULL, 8);
            f.size = size;
            if (name && s_putx(&s, name, &f, &err))
                send_words(stdout, 1, "OK");
            else
                answer_no(err ? err : "bad request");
            free(f.data);
        } else if (strcmp(w[0], "RUN") == 0 && n == 3) {
            size_t nw, insize;
            if (!parse_size(w[1], &nw) || !parse_size(w[2], &insize))
                break;
            struct uucmd cmd = { 0 };
            cmd.w = calloc(nw ? nw : 1, sizeof *cmd.w);
            bool bad = !cmd.w;
            char *wb = NULL;
            for (size_t i = 0; i < nw && !bad; i++) {
                char *ww[6];
                int k = read_words(stdin, &wb, ww, 6);
                if (k != 5 || strcmp(ww[0], "W") != 0 || kind_of(ww[1]) < 0) {
                    bad = true;
                    break;
                }
                char *t = dec(ww[2]), *sy = dec(ww[3]), *pa = dec(ww[4]);
                cmd.w[i].kind = kind_of(ww[1]);
                cmd.w[i].text = xstrdup(t ? t : "");
                cmd.w[i].sys = sy ? xstrdup(sy) : NULL;
                cmd.w[i].path = pa ? xstrdup(pa) : NULL;
                cmd.n = i + 1;
            }
            free(wb);
            struct uufile in = { 0 };
            if (bad || !read_data(stdin, insize, &in.data))
                break;
            in.size = insize;
            int status;
            struct uufile out = { 0 }, eo = { 0 }, *outfiles = NULL;
            if (s_run(&s, &cmd, &in, &status, &out, &eo, &outfiles, &err)) {
                size_t nout = 0;
                for (size_t i = 0; i < cmd.n; i++)
                    if (cmd.w[i].kind == W_OUT)
                        nout++;
                printf("OK %d %zu %zu %zu\n", status, out.size, eo.size, nout);
                fwrite(out.data, 1, out.size, stdout);
                fwrite(eo.data, 1, eo.size, stdout);
                for (size_t i = 0; i < nout; i++) {
                    if (outfiles[i].data) {
                        printf("F %zu\n", outfiles[i].size);
                        fwrite(outfiles[i].data, 1, outfiles[i].size, stdout);
                    } else {
                        fputs("F -\n", stdout);
                    }
                }
                uu_free_files(outfiles, nout);
                free(out.name);
                free(out.data);
                free(eo.name);
                free(eo.data);
            } else {
                answer_no(err ? err : "bad request");
            }
            for (size_t i = 0; i < cmd.n; i++) {
                free(cmd.w[i].text);
                free(cmd.w[i].sys);
                free(cmd.w[i].path);
            }
            free(cmd.w);
            free(in.data);
        } else {
            answer_no("unknown request");
        }
        free(err);
        if (fflush(stdout) != 0)
            break;
    }
    free(buf);
    end_session(&s);
    return 0;
}

/* ---- mail ---- */

bool uu_mail(const char *user, const char *subject, const char *body)
{
    int p[2];
    if (pipe(p) != 0)
        return false;
    char *cmd = xasprintf("%s \"$@\"", conf.mail);
    fflush(NULL);
    pid_t pid = fork();
    if (pid < 0) {
        close(p[0]);
        close(p[1]);
        free(cmd);
        return false;
    }
    if (pid == 0) {
        dup2(p[0], 0);
        close(p[0]);
        close(p[1]);
        int null = open("/dev/null", O_WRONLY);
        if (null >= 0)
            dup2(null, 1);
        execl("/bin/sh", "sh", "-c", cmd, "sh", "-s", subject, user, (char *)NULL);
        _exit(127);
    }
    free(cmd);
    close(p[0]);
    void (*old)(int) = signal(SIGPIPE, SIG_IGN);
    write_all(p[1], body, strlen(body));
    close(p[1]);
    signal(SIGPIPE, old);
    int st;
    while (waitpid(pid, &st, 0) < 0)
        if (errno != EINTR)
            return false;
    return WIFEXITED(st) && WEXITSTATUS(st) == 0;
}

/* ---- the spool ---- */

static char *spool_dir(const char *sub)
{
    char *d = xasprintf("%s/%s", conf.spool, sub);
    struct stat st;
    if (stat(d, &st) != 0) {
        mkdirs(conf.spool, 0755);
        if (mkdir(d, 0755) == 0)
            chmod(d, 01777);
    }
    return d;
}

char *uu_job_path(const char *id)
{
    char *d = spool_dir("jobs");
    char *p = xasprintf("%s/%s", d, id);
    free(d);
    return p;
}

char *uu_new_id(const char *sys)
{
    mkdirs(conf.spool, 0755);
    char *jobs = spool_dir("jobs");
    free(jobs);
    char *path = xasprintf("%s/SEQ", conf.spool);
    mode_t um = umask(0);
    int fd = open(path, O_RDWR | O_CREAT, 0666);
    umask(um);
    free(path);
    if (fd < 0)
        return NULL;
    if (lockf(fd, F_LOCK, 0) != 0) {
        close(fd);
        return NULL;
    }
    char buf[32] = { 0 };
    ssize_t r = read(fd, buf, sizeof buf - 1);
    long n = r > 0 ? atol(buf) : 0;
    n = n >= 9999999 ? 1 : n + 1;
    snprintf(buf, sizeof buf, "%ld\n", n);
    bool ok = lseek(fd, 0, SEEK_SET) == 0 && ftruncate(fd, 0) == 0 && write_all(fd, buf, strlen(buf));
    close(fd);
    if (!ok)
        return NULL;
    return xasprintf("%s.%04ld", sys, n);
}

char *uu_spool_data(const char *id, int k, const char *data, size_t size)
{
    char *d = spool_dir("data");
    char *p = xasprintf("%s/%s.%d", d, id, k);
    free(d);
    int fd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0 || !write_all(fd, data, size) || close(fd) != 0) {
        if (fd >= 0)
            unlink(p);
        free(p);
        return NULL;
    }
    return p;
}

static void put_words(FILE *f, int n, ...)
{
    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        const char *s = va_arg(ap, const char *);
        char *e = enc(s);
        fprintf(f, "%s%s", i ? " " : "", e);
        free(e);
    }
    va_end(ap);
    putc('\n', f);
}

bool uu_save(const struct uujob *j)
{
    char *path = uu_job_path(j->id);
    char *tmp = xasprintf("%s.tmp", path);
    char *slash = strrchr(tmp, '/');
    /* the temporary file starts with "." */
    char *t2 = xasprintf("%.*s/.%s", (int)(slash - tmp), tmp, slash + 1);
    free(tmp);
    tmp = t2;
    FILE *f = fopen(tmp, "w");
    if (!f) {
        free(path);
        free(tmp);
        return false;
    }
    fchmod(fileno(f), 0644);
    char t[32], a[16];
    snprintf(t, sizeof t, "%lld", (long long)j->time);
    snprintf(a, sizeof a, "%d", j->attempts);
    put_words(f, 2, "id", j->id);
    put_words(f, 2, "user", j->user);
    put_words(f, 2, "system", j->system);
    put_words(f, 2, "time", t);
    put_words(f, 2, "flags", j->flags ? j->flags : "");
    if (j->notify)
        put_words(f, 2, "notify", j->notify);
    put_words(f, 2, "attempts", a);
    if (j->error)
        put_words(f, 2, "error", j->error);
    for (size_t i = 0; i < j->nitems; i++) {
        const struct uuitem *it = &j->items[i];
        put_words(f, 6, "item", it->ssys, it->spath, it->dsys, it->dpath, it->data);
    }
    if (j->xsys) {
        put_words(f, 2, "x", j->xsys);
        for (size_t i = 0; i < j->cmd.n; i++) {
            const struct uuword *w = &j->cmd.w[i];
            put_words(f, 5, "w", kind_name(w->kind), w->text, w->sys, w->path);
        }
        if (j->indata)
            put_words(f, 2, "indata", j->indata);
        if (j->line)
            put_words(f, 2, "line", j->line);
    }
    bool ok = fflush(f) == 0 && !ferror(f);
    if (fclose(f) != 0)
        ok = false;
    if (ok && rename(tmp, path) != 0)
        ok = false;
    if (!ok)
        unlink(tmp);
    free(tmp);
    free(path);
    return ok;
}

static char *dupdec(char *s)
{
    char *d = dec(s);
    return d ? xstrdup(d) : NULL;
}

struct uujob *uu_load(const char *id)
{
    if (!*id || strchr(id, '/') || id[0] == '.')
        return NULL;
    char *path = uu_job_path(id);
    FILE *f = fopen(path, "r");
    free(path);
    if (!f)
        return NULL;
    struct uujob *j = calloc(1, sizeof *j);
    char *buf = NULL, *w[8];
    int n;
    while ((n = read_words(f, &buf, w, 8)) >= 0) {
        if (n < 2)
            continue;
        const char *k = w[0];
        if (strcmp(k, "id") == 0)
            j->id = dupdec(w[1]);
        else if (strcmp(k, "user") == 0)
            j->user = dupdec(w[1]);
        else if (strcmp(k, "system") == 0)
            j->system = dupdec(w[1]);
        else if (strcmp(k, "time") == 0)
            j->time = (time_t)atoll(w[1]);
        else if (strcmp(k, "flags") == 0)
            j->flags = dupdec(w[1]);
        else if (strcmp(k, "notify") == 0)
            j->notify = dupdec(w[1]);
        else if (strcmp(k, "attempts") == 0)
            j->attempts = atoi(w[1]);
        else if (strcmp(k, "error") == 0)
            j->error = dupdec(w[1]);
        else if (strcmp(k, "item") == 0 && n == 6) {
            j->items = xrealloc(j->items, (j->nitems + 1) * sizeof *j->items);
            j->items[j->nitems++] = (struct uuitem){ dupdec(w[1]), dupdec(w[2]), dupdec(w[3]), dupdec(w[4]),
                                                     dupdec(w[5]) };
        } else if (strcmp(k, "x") == 0)
            j->xsys = dupdec(w[1]);
        else if (strcmp(k, "w") == 0 && n == 5) {
            j->cmd.w = xrealloc(j->cmd.w, (j->cmd.n + 1) * sizeof *j->cmd.w);
            int kind = kind_of(w[1]);
            j->cmd.w[j->cmd.n++] = (struct uuword){ kind < 0 ? W_LIT : kind, dupdec(w[2]), dupdec(w[3]),
                                                    dupdec(w[4]) };
        } else if (strcmp(k, "indata") == 0)
            j->indata = dupdec(w[1]);
        else if (strcmp(k, "line") == 0)
            j->line = dupdec(w[1]);
    }
    free(buf);
    fclose(f);
    if (!j->id || !j->user || !j->system) {
        uu_free_job(j);
        return NULL;
    }
    if (!j->flags)
        j->flags = xstrdup("");
    return j;
}

void uu_free_job(struct uujob *j)
{
    if (!j)
        return;
    free(j->id);
    free(j->user);
    free(j->system);
    free(j->flags);
    free(j->notify);
    free(j->error);
    for (size_t i = 0; i < j->nitems; i++) {
        free(j->items[i].ssys);
        free(j->items[i].spath);
        free(j->items[i].dsys);
        free(j->items[i].dpath);
        free(j->items[i].data);
    }
    free(j->items);
    free(j->xsys);
    for (size_t i = 0; i < j->cmd.n; i++) {
        free(j->cmd.w[i].text);
        free(j->cmd.w[i].sys);
        free(j->cmd.w[i].path);
    }
    free(j->cmd.w);
    free(j->indata);
    free(j->line);
    free(j);
}

bool uu_remove(const struct uujob *j)
{
    for (size_t i = 0; i < j->nitems; i++)
        if (j->items[i].data)
            unlink(j->items[i].data);
    if (j->indata)
        unlink(j->indata);
    char *p = uu_job_path(j->id);
    bool ok = unlink(p) == 0;
    free(p);
    return ok;
}

static long seq_of(const char *id)
{
    const char *dot = strrchr(id, '.');
    return dot ? atol(dot + 1) : 0;
}

static int by_seq(const void *a, const void *b)
{
    const char *x = *(char *const *)a, *y = *(char *const *)b;
    long p = seq_of(x), q = seq_of(y);
    if (p != q)
        return p < q ? -1 : 1;
    return strcmp(x, y);
}

char **uu_jobs(size_t *n)
{
    *n = 0;
    char *dir = xasprintf("%s/jobs", conf.spool);
    DIR *d = opendir(dir);
    free(dir);
    if (!d)
        return NULL;
    char **v = NULL;
    struct dirent *e;
    while ((e = readdir(d)) != NULL)
        if (e->d_name[0] != '.')
            push(&v, n, e->d_name);
    closedir(d);
    if (*n)
        qsort(v, *n, sizeof *v, by_seq);
    return v;
}

void uu_status(const char *sys, const char *what)
{
    char *d = spool_dir("status");
    char *p = xasprintf("%s/%s", d, sys);
    free(d);
    FILE *f = fopen(p, "w");
    if (f) {
        fprintf(f, "%lld %s\n", (long long)time(NULL), what);
        fclose(f);
        chmod(p, 0666);
    }
    free(p);
}

char *uu_get_status(const char *sys)
{
    char *p = xasprintf("%s/status/%s", conf.spool, sys);
    size_t size;
    char *s = uu_slurp(p, &size);
    free(p);
    if (!s)
        return NULL;
    s = xrealloc(s, size + 1);
    s[size] = '\0';
    char *nl = strchr(s, '\n');
    if (nl)
        *nl = '\0';
    return s;
}

/* ---- running jobs ---- */

struct conns {
    struct conn *c[8];
    size_t n;
};

static struct conn *conn_for(struct conns *cs, const char *sys, char **err)
{
    const char *name = uu_is_local(sys) ? conf.nodename : sys;
    for (size_t i = 0; i < cs->n; i++)
        if (strcmp(cs->c[i]->name, name) == 0)
            return cs->c[i];
    struct conn *c = uu_connect(sys, err);
    if (!c)
        return NULL;
    if (cs->n == sizeof cs->c / sizeof cs->c[0]) {
        uu_disconnect(cs->c[0]);
        memmove(cs->c, cs->c + 1, (cs->n - 1) * sizeof cs->c[0]);
        cs->n--;
    }
    cs->c[cs->n++] = c;
    return c;
}

static void close_all(struct conns *cs)
{
    for (size_t i = 0; i < cs->n; i++)
        uu_disconnect(cs->c[i]);
    cs->n = 0;
}

/* a problem reaching a system: temporary */
static bool unreachable(const char *err)
{
    return strncmp(err, "cannot reach ", 13) == 0 || strstr(err, "the connection was lost") != NULL;
}

char *uu_describe(const struct uujob *j)
{
    if (j->xsys)
        return xasprintf("execute %s", j->line ? j->line : "?");
    size_t cap = 64, len = 0;
    char *s = xmalloc(cap);
    s[0] = '\0';
    for (size_t i = 0; i < j->nitems; i++) {
        const struct uuitem *it = &j->items[i];
        char *one = xasprintf("%scopy %s%s%s to %s%s%s", i ? "; " : "", it->ssys ? it->ssys : "",
                              it->ssys ? "!" : "", it->spath, it->dsys ? it->dsys : "", it->dsys ? "!" : "",
                              it->dpath);
        size_t l = strlen(one);
        if (len + l + 1 > cap) {
            cap = (len + l + 1) * 2;
            s = xrealloc(s, cap);
        }
        memcpy(s + len, one, l + 1);
        len += l;
        free(one);
    }
    return s;
}

static bool run_copy(struct uujob *j, struct conns *cs, bool *temporary, char **err)
{
    while (j->nitems > 0) {
        struct uuitem *it = &j->items[0];
        struct uufile *files = NULL;
        size_t n = 0;
        if (it->data) {
            size_t size;
            char *d = uu_slurp(it->data, &size);
            if (!d) {
                *err = xasprintf("%s: %s", it->data, strerror(errno));
                return false;
            }
            files = xmalloc(sizeof *files);
            files[0] = (struct uufile){ xstrdup(it->spath), 0644, d, size };
            struct stat st;
            if (stat(it->data, &st) == 0)
                files[0].mode = st.st_mode & 0777;
            n = 1;
        } else {
            struct conn *c = conn_for(cs, it->ssys, err);
            if (!c) {
                *temporary = true;
                return false;
            }
            if (!uu_get(c, it->spath, &files, &n, err)) {
                *temporary = unreachable(*err);
                return false;
            }
        }
        struct conn *c = conn_for(cs, it->dsys, err);
        if (!c) {
            uu_free_files(files, n);
            *temporary = true;
            return false;
        }
        char *flags = xasprintf("%s%s", j->flags, n > 1 ? "D" : "");
        for (size_t i = 0; i < n; i++) {
            char *final = NULL;
            if (!uu_put(c, it->dpath, files[i].mode, flags, j->notify, &files[i], &final, err)) {
                *temporary = unreachable(*err);
                free(flags);
                uu_free_files(files, n);
                return false;
            }
            free(final);
        }
        free(flags);
        uu_free_files(files, n);
        /* done: off the job */
        if (it->data)
            unlink(it->data);
        free(it->ssys);
        free(it->spath);
        free(it->dsys);
        free(it->dpath);
        free(it->data);
        memmove(j->items, j->items + 1, (j->nitems - 1) * sizeof *j->items);
        j->nitems--;
        if (j->nitems > 0)
            uu_save(j);
    }
    return true;
}

static bool run_exec(struct uujob *j, struct conns *cs, bool *temporary, char **err, int *status)
{
    struct conn *x = conn_for(cs, j->xsys, err);
    if (!x) {
        *temporary = true;
        return false;
    }
    /* the files to gather to where it runs */
    for (size_t i = 0; i < j->cmd.n; i++) {
        struct uuword *w = &j->cmd.w[i];
        if (w->kind != W_GATHER)
            continue;
        struct conn *c = conn_for(cs, w->sys, err);
        if (!c) {
            *temporary = true;
            return false;
        }
        struct uufile *files;
        size_t n;
        if (!uu_get(c, w->path, &files, &n, err)) {
            *temporary = unreachable(*err);
            return false;
        }
        if (n != 1) {
            *err = xasprintf("%s!%s: not one file", w->sys ? w->sys : "", w->path);
            uu_free_files(files, n);
            return false;
        }
        bool ok = uu_putx(x, w->text, &files[0], err);
        uu_free_files(files, n);
        if (!ok) {
            *temporary = unreachable(*err);
            return false;
        }
    }
    struct uufile in = { 0 }, *inp = NULL;
    if (j->indata) {
        in.data = uu_slurp(j->indata, &in.size);
        if (!in.data) {
            *err = xasprintf("%s: %s", j->indata, strerror(errno));
            return false;
        }
        in.name = xstrdup("standard input");
        inp = &in;
    }
    struct uufile out = { 0 }, *outfiles = NULL;
    bool ok = uu_run(x, &j->cmd, inp, status, &out, &outfiles, err);
    free(in.name);
    free(in.data);
    if (!ok) {
        *temporary = unreachable(*err);
        return false;
    }
    /* the output files, where they go */
    size_t k = 0;
    for (size_t i = 0; i < j->cmd.n && ok; i++) {
        struct uuword *w = &j->cmd.w[i];
        if (w->kind != W_OUT)
            continue;
        struct uufile *f = &outfiles[k++];
        if (!f->data) {
            *err = xasprintf("%s: not made", w->text);
            ok = false;
            break;
        }
        free(f->name);
        f->name = xstrdup(w->text);
        struct conn *c = conn_for(cs, w->sys, err);
        char *final = NULL;
        if (!c || !uu_put(c, w->path, 0644, "d", NULL, f, &final, err))
            ok = false;
        free(final);
    }
    uu_free_files(outfiles, k);
    free(out.name);
    free(out.data);
    return ok;
}

bool uu_run_job(struct uujob *j, bool *temporary, char **err)
{
    struct conns cs = { { 0 }, 0 };
    *temporary = false;
    *err = NULL;
    int status = 0;
    char *what = uu_describe(j);
    bool ok = j->xsys ? run_exec(j, &cs, temporary, err, &status) : run_copy(j, &cs, temporary, err);
    close_all(&cs);
    if (ok && j->xsys && status != 0) {
        /* it ran, and failed */
        if (!strchr(j->flags, 'n')) {
            char *body = xasprintf("Job %s (%s) on %s exited with status %d.\n", j->id, what, j->xsys, status);
            char *subject = xasprintf("uux job %s failed", j->id);
            uu_mail(j->user, subject, body);
            free(subject);
            free(body);
        }
    }
    if (ok) {
        if (strchr(j->flags, 'm')) {
            char *body = xasprintf("Job %s (%s) is done.\n", j->id, what);
            char *subject = xasprintf("uucp job %s done", j->id);
            uu_mail(j->user, subject, body);
            free(subject);
            free(body);
        }
        uu_remove(j);
    } else if (*temporary) {
        j->attempts++;
        free(j->error);
        j->error = xstrdup(*err ? *err : "failed");
        uu_save(j);
    } else {
        bool mail = j->xsys ? !strchr(j->flags, 'n') : strchr(j->flags, 'm') != NULL;
        if (mail) {
            char *body = xasprintf("Job %s (%s) failed: %s\n", j->id, what, *err ? *err : "?");
            char *subject = xasprintf("%s job %s failed", j->xsys ? "uux" : "uucp", j->id);
            uu_mail(j->user, subject, body);
            free(subject);
            free(body);
        }
        uu_remove(j);
    }
    free(what);
    return ok;
}
