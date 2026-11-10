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
 * spool.h - the at spool: where jobs are kept, and what a job file holds
 * (shared by at, batch and atd)
 */

#ifndef SPOOL_H
#define SPOOL_H

#include <stdbool.h>
#include <sys/types.h>
#include <time.h>

/* the directories (make can set them: -DAT_SPOOL=... -DAT_CONFDIR=...) */
#ifndef AT_SPOOL
#define AT_SPOOL "/var/spool/posixat"
#endif
#ifndef AT_CONFDIR
#define AT_CONFDIR "/etc"
#endif

/* the spool directory and the directory of at.allow and at.deny: the
   environment variables AT_SPOOL and AT_CONFDIR when the program is not
   set-user-ID or set-group-ID (so tests and private installations can use
   their own), else the compiled-in ones */
const char *spool_dir(void);
const char *conf_dir(void);

/* a job: the file <spool>/<id>, id "<number>.<queue letter>"; the first
   lines are comments with the fields, up to "# end" */
struct job {
    char id[64];
    uid_t owner;
    time_t when;
    char queue;
    bool mail;
    char shell[1024];
};

/* read the fields of the job file path; false if it is not a job file */
bool job_read(const char *path, struct job *j);

/* is the job id well formed? */
bool job_id_ok(const char *id);

/* the time as at writes it: "%a %b %e %T %Y" (the POSIX locale's names) */
void job_date(char *buf, size_t size, time_t t);

/* is the user allowed by <conf>/<base>.allow and <base>.deny? */
bool user_allowed(const char *base, const char *user, bool privileged);

#endif
