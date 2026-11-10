/*
 * mkutx.c - write a file of struct utmpx records for the tests of who,
 * write and talk (the layout is the system's, as utx.c reads it)
 *
 *   mkutx < spec > file
 *
 * Each line of spec: TYPE USER LINE ID PID TIME [HOST [TERM EXIT]], with
 * "-" for an empty string; TYPE one of USER LOGIN INIT DEAD BOOT OLD NEW
 * RUNLVL EMPTY; TIME seconds since the Epoch, or -N for N seconds ago.
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <utmpx.h>

static void put(char *dst, size_t n, const char *s)
{
    memset(dst, 0, n);
    if (strcmp(s, "-") != 0)
        strncpy(dst, s, n);
}

int main(void)
{
    char line[1024];
    while (fgets(line, sizeof line, stdin)) {
        char ty[32], user[256], ln[256], id[32], host[256] = "-";
        long pid, t;
        int term = 0, ex = 0;
        int n = sscanf(line, "%31s %255s %255s %31s %ld %ld %255s %d %d", ty, user, ln, id, &pid, &t,
                       host, &term, &ex);
        if (n < 6) {
            fprintf(stderr, "mkutx: bad line: %s", line);
            return 1;
        }
        struct utmpx x;
        memset(&x, 0, sizeof x);
        static const struct { const char *n; int v; } types[] = {
            { "USER", USER_PROCESS }, { "LOGIN", LOGIN_PROCESS }, { "INIT", INIT_PROCESS },
            { "DEAD", DEAD_PROCESS }, { "BOOT", BOOT_TIME }, { "OLD", OLD_TIME },
            { "NEW", NEW_TIME }, { "RUNLVL", 1 }, { "EMPTY", EMPTY },
        };
        int v = -1;
        for (size_t i = 0; i < sizeof types / sizeof *types; i++)
            if (strcmp(ty, types[i].n) == 0)
                v = types[i].v;
        if (v < 0) {
            fprintf(stderr, "mkutx: bad type: %s\n", ty);
            return 1;
        }
        x.ut_type = (short)v;
        put(x.ut_user, sizeof x.ut_user, user);
        put(x.ut_line, sizeof x.ut_line, ln);
        put(x.ut_id, sizeof x.ut_id, id);
        x.ut_pid = (pid_t)pid;
        if (t < 0)
            t = (long)time(NULL) + t;
        x.ut_tv.tv_sec = t;
#if defined(__GLIBC__) || defined(__UT_HOSTSIZE)
        put(x.ut_host, sizeof x.ut_host, host);
        x.ut_exit.__e_termination = (short)term;
        x.ut_exit.__e_exit = (short)ex;
#else
        (void)term, (void)ex;
#endif
        fwrite(&x, sizeof x, 1, stdout);
    }
    return fflush(stdout) != 0;
}
