/*
 * utx.h - the user accounting database (utmpx), read through
 * getutxent() or from a file of struct utmpx records; and the terminals
 * of its lines (shared by who, write, talk and lp)
 */

#ifndef UTX_H
#define UTX_H

#include <stdbool.h>
#include <sys/types.h>
#include <time.h>

struct utx {
    int type;                   /* ut_type */
    char user[260], line[260], id[16], host[300];
    pid_t pid;
    time_t time;
    bool has_exit;              /* the next two are known */
    int term, exit;             /* of a DEAD_PROCESS */
};

/* start reading the database: file names a file of records, else the
   file in $POSIX_UTMPX (a hook for tests, not when set-user-ID or
   set-group-ID), else the system's database; false with errno set */
bool utx_open(const char *file);
/* the next entry; false at the end (errno set on a read error, else 0) */
bool utx_next(struct utx *u);
void utx_close(void);

/* the directory of the terminal lines: $POSIX_DEVDIR (as above), or /dev */
const char *utx_devdir(void);
/* the pathname of a line (malloc'd): itself when absolute */
char *utx_ttypath(const char *line);
/* a terminal's name as in the database: its pathname less the directory
   of the lines, or less /dev (pointing into path) */
const char *utx_linename(const char *path);
/* the value of an environment variable for a hook, or NULL */
const char *utx_hook(const char *name);

#endif
