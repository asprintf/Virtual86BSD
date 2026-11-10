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
 * cron.h - the crontab spool, and the entries of a crontab (shared by
 * crontab and crond)
 */

#ifndef CRON_H
#define CRON_H

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#ifndef CRON_SPOOL
#define CRON_SPOOL "/var/spool/posixcron"
#endif
#ifndef CRON_CONFDIR
#define CRON_CONFDIR "/etc"
#endif

/* the spool directory (a file per user, named by the login name) and the
   directory of cron.allow and cron.deny: CRON_SPOOL and CRON_CONFDIR of
   the environment when the program is not set-user-ID or set-group-ID */
const char *cron_spool(void);
const char *cron_confdir(void);

/* an entry: the five patterns as bit sets, and the command */
struct entry {
    unsigned long long minute;  /* bits 0-59 */
    unsigned long hour;         /* 0-23 */
    unsigned long mday;         /* 1-31 */
    unsigned long month;        /* 1-12 */
    unsigned long wday;         /* 0-6 */
    bool mday_star, month_star, wday_star;
    char *command;              /* the sixth field, as written */
};

/* parse a line: 1 an entry, 0 a blank or comment line, -1 an error (a
   message in err) */
int cron_parse(const char *line, struct entry *e, char *err, size_t errsize);

/* does the entry's time match tm? */
bool cron_match(const struct entry *e, const struct tm *tm);

/* the command field: the command (up to the first '%' not after a
   backslash) and the standard input (the rest, each such '%' a newline,
   and a newline at the end); "\%" is a '%', other backslashes are kept
   (for the shell); malloc()ed */
void cron_command(const char *field, char **cmd, char **input);

/* is the user allowed by cron.allow and cron.deny? */
bool cron_allowed(const char *user, bool privileged);

#endif
