/* Internal: calendar arithmetic and the time-zone database. */
#ifndef _INTERNAL_TIME_IMPL_H
#define _INTERNAL_TIME_IMPL_H
#include <time.h>

/* seconds since the Epoch <-> broken-down UTC; 0 / -1 (EOVERFLOW) */
int  __secs_to_tm(long long t, struct tm *tm);
long long __tm_to_secs(const struct tm *tm);   /* normalises nothing; any field ranges */
long long __days_from_civil(long long y, unsigned m, unsigned d);

/* The current zone (TZ, else /etc/localtime, else UTC).  __tz_lookup
 * gives the offset east of UTC, the DST flag and the abbreviation in
 * effect at UTC time t. */
void __tz_lock(void);
void __tz_unlock(void);
void __tz_update(void);         /* re-read TZ if it changed; call with the lock */
void __tz_lookup(long long t, long *off, int *isdst, const char **abbr);
/* offsets of standard and daylight time near UTC time t (dst == std if none) */
void __tz_offsets(long long t, long *stdoff, long *dstoff);

extern char *__tzname[2];
extern long __timezone;
extern int __daylight;

struct tm *__localtime_r(const time_t *, struct tm *);
struct tm *__gmtime_r(const time_t *, struct tm *);
size_t __strftime_l(char *, size_t, const char *, const struct tm *, void *);
#endif
