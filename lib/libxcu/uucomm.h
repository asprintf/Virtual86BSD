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
 * uucomm.h - the UUCP system of uucp, uux, uustat and uucico: the
 * configuration, the spool, jobs, and the conversation with the other
 * systems
 */

#ifndef UUCOMM_H
#define UUCOMM_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <time.h>

extern const char *progname;

void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);
char *xasprintf(const char *fmt, ...);

/* ---- the configuration ---- */

struct uusystem {
    char *name;
    char *command;              /* a shell command reaching its uucico -S */
};

struct uuconf {
    char *nodename;             /* this system */
    char *spool;                /* the spool directory */
    char *pubdir;               /* the public directory */
    char *mail;                 /* the command that sends mail */
    char **read, **write;       /* what other systems may read and write */
    size_t nread, nwrite;
    char **commands;            /* what other systems may run ("ALL": any) */
    size_t ncommands;
    struct uusystem *systems;
    size_t nsystems;
};

extern struct uuconf conf;

/* read the configuration (UUCP_CONF, if not set-user-ID; otherwise
   /etc/uucp/config); false (with a diagnostic) if it cannot be read */
bool uu_config(void);
/* is name this system ("" is too)? */
bool uu_is_local(const char *name);
/* a system that is known (this one or one of the configuration)? */
bool uu_known(const char *name);
/* the user running the program */
const char *uu_user(void);

/* ---- paths ---- */

/* name!path: the system (NULL: none given) and the path */
void uu_split(const char *arg, char **sys, char **path);
/* a path as the requester gives it: ~ kept, others made absolute (by
   the current directory) */
char *uu_absolute(const char *path);
/* a path on this system: ~user, ~/ expanded */
char *uu_expand(const char *path);
/* may a remote system read (write) path? */
bool uu_allowed(const char *path, bool write);
/* a valid system name: letters, digits, "-", "_", "." */
bool uu_sysname(const char *s);

/* ---- the conversation ---- */

struct uufile {
    char *name;                 /* the path it has there */
    mode_t mode;
    char *data;
    size_t size;
};

/* the words of a command for uux: what each is */
enum { W_LIT, W_PATH, W_GATHER, W_OUT, W_OP };

struct uuword {
    int kind;
    char *text;                 /* W_LIT, W_OP: the text; W_PATH: the path
                                   on the system that runs; W_GATHER,
                                   W_OUT: the name in the directory where
                                   it runs */
    char *sys, *path;           /* W_GATHER, W_OUT: where it comes from or
                                   goes to */
};

struct uucmd {
    struct uuword *w;
    size_t n;
};

struct conn;

/* connect to a system (this one: no other process); NULL with *err */
struct conn *uu_connect(const char *sys, char **err);
void uu_disconnect(struct conn *c);
/* send a file; flags: "d" make directories, "f" not, "D" the
   destination is a directory; notify: a user to tell (or NULL).  The
   path the file got: *final */
bool uu_put(struct conn *c, const char *dest, mode_t mode, const char *flags, const char *notify,
            const struct uufile *f, char **final, char **err);
/* get the files a path (a pattern) names */
bool uu_get(struct conn *c, const char *pattern, struct uufile **files, size_t *n, char **err);
/* put a file in the directory where commands run */
bool uu_putx(struct conn *c, const char *name, const struct uufile *f, char **err);
/* run a command there: its exit status, its output, the files it made
   (the W_OUT words, in order; NULL data if not made) */
bool uu_run(struct conn *c, const struct uucmd *cmd, const struct uufile *in, int *status,
            struct uufile *out, struct uufile **outfiles, char **err);
void uu_free_files(struct uufile *f, size_t n);

/* uucico -S: answer a requester on the standard input and output */
int uu_slave(void);

/* ---- mail ---- */

/* send mail to user (on this system); false if it could not be sent */
bool uu_mail(const char *user, const char *subject, const char *body);

/* ---- jobs ---- */

struct uuitem {
    char *ssys, *spath;         /* from */
    char *dsys, *dpath;         /* to */
    char *data;                 /* the copy in the spool (-C), or NULL */
};

struct uujob {
    char *id, *user, *system;
    time_t time;
    char *flags;                /* m: mail; d, f: directories; D: the
                                   destination is a directory; n: no
                                   notification (uux) */
    char *notify;               /* uucp -n */
    char *error;                /* the last attempt's problem */
    int attempts;
    /* uucp */
    struct uuitem *items;
    size_t nitems;
    /* uux */
    char *xsys;                 /* where it runs; NULL: not uux */
    struct uucmd cmd;
    char *indata;               /* the standard input (-p), spooled */
    char *line;                 /* the command as given */
};

/* a new job id for a system (with a sequence number of the spool) */
char *uu_new_id(const char *sys);
/* write (rewrite) a job in the spool */
bool uu_save(const struct uujob *j);
/* read a job of the spool; NULL if there is none */
struct uujob *uu_load(const char *id);
void uu_free_job(struct uujob *j);
/* remove a job and its spooled files */
bool uu_remove(const struct uujob *j);
/* the ids of the jobs in the spool, in order */
char **uu_jobs(size_t *n);
/* spool a copy of data; its path */
char *uu_spool_data(const char *id, int k, const char *data, size_t size);
/* the path of a job's file */
char *uu_job_path(const char *id);

/* run a job: true when done (removed from the spool), false when not
   (queued again: *temporary; or failed for good: removed), with a
   diagnostic in *err */
bool uu_run_job(struct uujob *j, bool *temporary, char **err);
/* what a job does, in a line */
char *uu_describe(const struct uujob *j);
/* record the result of an attempt to reach a system */
void uu_status(const char *sys, const char *what);
/* the last status of a system: "time text", or NULL */
char *uu_get_status(const char *sys);

/* read a whole file; NULL (errno) if it cannot be */
char *uu_slurp(const char *path, size_t *size);

#endif
