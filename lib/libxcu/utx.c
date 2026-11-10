/*
 * utx.c - the user accounting database (see utx.h)
 *
 * A named file is read as records of the struct utmpx of <utmpx.h>, the
 * layout of the system's own database on the systems where it is a file.
 * The exit status of a dead process is not in the struct utmpx of POSIX;
 * it is taken where the C library is known to have it (glibc).
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include "utx.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmpx.h>

static FILE *uf;                /* the named file, or NULL: getutxent() */
static bool usedb;

const char *utx_hook(const char *name)
{
    if (getuid() != geteuid() || getgid() != getegid())
        return NULL;
    const char *v = getenv(name);
    return v && *v ? v : NULL;
}

bool utx_open(const char *file)
{
    if (!file)
        file = utx_hook("POSIX_UTMPX");
    if (file) {
        uf = fopen(file, "rb");
        return uf != NULL;
    }
    usedb = true;
    setutxent();
    return true;
}

static void copy(char *dst, const char *src, size_t n)
{
    size_t l = 0;
    while (l < n && src[l])
        l++;
    memcpy(dst, src, l);
    dst[l] = '\0';
}

static void fill(struct utx *u, const struct utmpx *x)
{
    u->type = x->ut_type;
    copy(u->user, x->ut_user, sizeof x->ut_user);
    copy(u->line, x->ut_line, sizeof x->ut_line);
    copy(u->id, x->ut_id, sizeof x->ut_id);
#if defined(__GLIBC__) || defined(__UT_HOSTSIZE)
    copy(u->host, x->ut_host, sizeof x->ut_host);
    u->has_exit = true;
    u->term = x->ut_exit.__e_termination;
    u->exit = x->ut_exit.__e_exit;
#else
    u->host[0] = '\0';
    u->has_exit = false;
    u->term = u->exit = 0;
#endif
    u->pid = x->ut_pid;
    u->time = (time_t)x->ut_tv.tv_sec;
}

bool utx_next(struct utx *u)
{
    errno = 0;
    if (uf) {
        struct utmpx x;
        size_t n = fread(&x, sizeof x, 1, uf);
        if (n != 1) {
            if (!ferror(uf))
                errno = 0;
            else if (errno == 0)
                errno = EIO;
            return false;
        }
        fill(u, &x);
        return true;
    }
    if (!usedb)
        return false;
    struct utmpx *x = getutxent();
    if (!x) {
        errno = 0;
        return false;
    }
    fill(u, x);
    return true;
}

void utx_close(void)
{
    if (uf) {
        fclose(uf);
        uf = NULL;
    }
    if (usedb) {
        endutxent();
        usedb = false;
    }
}

const char *utx_devdir(void)
{
    const char *d = utx_hook("POSIX_DEVDIR");
    return d ? d : "/dev";
}

char *utx_ttypath(const char *line)
{
    const char *d = utx_devdir();
    size_t a = strlen(d), b = strlen(line);
    char *p = malloc(a + b + 2);
    if (!p)
        return NULL;
    if (line[0] == '/') {
        memcpy(p, line, b + 1);
        return p;
    }
    memcpy(p, d, a);
    p[a] = '/';
    memcpy(p + a + 1, line, b + 1);
    return p;
}

const char *utx_linename(const char *path)
{
    const char *d = utx_devdir();
    size_t a = strlen(d);
    if (strncmp(path, d, a) == 0 && path[a] == '/')
        return path + a + 1;
    if (strncmp(path, "/dev/", 5) == 0)
        return path + 5;
    return path;
}
