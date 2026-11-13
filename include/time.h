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

#ifndef _TIME_H
#define _TIME_H
#include <bits/features.h>
#include <bits/null.h>
#define __NEED_size_t
#define __NEED_time_t
#define __NEED_clock_t
#define __NEED_struct_timespec
#if __POSIX_VISIBLE
#define __NEED_clockid_t
#define __NEED_timer_t
#define __NEED_pid_t
#define __NEED_locale_t
#endif
#include <bits/alltypes.h>

struct tm {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
#if __POSIX_VISIBLE
	long tm_gmtoff;
	const char *tm_zone;
#else
	long __tm_gmtoff;
	const char *__tm_zone;
#endif
};

#define CLOCKS_PER_SEC ((clock_t)1000000)
#define TIME_UTC 1

#if __POSIX_VISIBLE
struct itimerspec {
	struct timespec it_interval;
	struct timespec it_value;
};

struct sigevent;

#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3
#define TIMER_ABSTIME            1
#endif

__BEGIN_DECLS
clock_t    clock(void);
double     difftime(time_t, time_t);
struct tm *gmtime(const time_t *);
struct tm *localtime(const time_t *);
time_t     mktime(struct tm *);
size_t     strftime(char *__RESTRICT, size_t, const char *__RESTRICT, const struct tm *__RESTRICT);
time_t     time(time_t *);
char      *asctime(const struct tm *);
char      *ctime(const time_t *);
#if __C11_VISIBLE
int        timespec_get(struct timespec *, int);
#endif

#if __POSIX_VISIBLE
int        clock_getcpuclockid(pid_t, clockid_t *);
int        clock_getres(clockid_t, struct timespec *);
int        clock_gettime(clockid_t, struct timespec *);
int        clock_nanosleep(clockid_t, int, const struct timespec *, struct timespec *);
int        clock_settime(clockid_t, const struct timespec *);
struct tm *gmtime_r(const time_t *__RESTRICT, struct tm *__RESTRICT);
struct tm *localtime_r(const time_t *__RESTRICT, struct tm *__RESTRICT);
int        nanosleep(const struct timespec *, struct timespec *);
size_t     strftime_l(char *__RESTRICT, size_t, const char *__RESTRICT, const struct tm *__RESTRICT, locale_t);
int        timer_create(clockid_t, struct sigevent *__RESTRICT, timer_t *__RESTRICT);
int        timer_delete(timer_t);
int        timer_getoverrun(timer_t);
int        timer_gettime(timer_t, struct itimerspec *);
int        timer_settime(timer_t, int, const struct itimerspec *__RESTRICT, struct itimerspec *__RESTRICT);
void       tzset(void);
extern char *tzname[2];
#endif
#if __POSIX_RANGE(199506L, 202405L)
/* POSIX.1c to POSIX.1-2008 (obsolescent there); removed in POSIX.1-2024 */
char      *asctime_r(const struct tm *__RESTRICT, char *__RESTRICT);
char      *ctime_r(const time_t *, char *);
#endif
#if __POSIX_RANGE(1L, 200112L)
/* POSIX.1-1990 and SUSv2; sysconf(_SC_CLK_TCK) */
#define CLK_TCK ((clock_t)100)
#endif
#if __XSI_VISIBLE
struct tm *getdate(const char *);
char      *strptime(const char *__RESTRICT, const char *__RESTRICT, struct tm *__RESTRICT);
extern int  daylight;
extern long timezone;
int *__getdate_err_location(void);
#define getdate_err (*__getdate_err_location())
#endif
__END_DECLS
#endif
