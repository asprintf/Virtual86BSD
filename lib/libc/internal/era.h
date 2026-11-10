/* Eras and alternative digits of LC_TIME, for strftime() and strptime(). */
#ifndef _ERA_H
#define _ERA_H
#include <stdint.h>
#include "locale_impl.h"

/* LC_TIME elements */
enum { T_ABDAY = 0, T_DAY = 7, T_ABMON = 14, T_MON = 26, T_AM = 38, T_PM = 39, T_DTFMT = 40,
       T_DFMT = 41, T_TFMT = 42, T_TFMTAMPM = 43, T_ERA_DFMT = 46, T_ALTDIGITS = 47,
       T_ERA_DTFMT = 48, T_ERA_TFMT = 49, T_ERA_N = 50, T_ERA_ENTRIES = 51,
       T_ALTMON = 111, T_ABALTMON = 135 };

/* an era (glibc's binary entries): the start and end dates as year - 1900,
 * month, day; dir is +1 when the years count up from start, else -1 */
struct __era { int dir, offset; int32_t start[3], stop[3]; const char *name, *format; };

unsigned __era_count(const struct __lcat *);
void __era_get(const struct __lcat *, unsigned, struct __era *);
int __era_has(const struct __era *, const int32_t date[3]);
/* whether y can be the era year (%Ey) of e */
int __era_year_ok(const struct __era *, long long y);

/* the alternative digits for v in 0..99, or 0 */
const char *__alt_digit(const struct __lcat *, long long v);
/* the value of the longest alternative digits at *s, which it skips; or -1 */
int __alt_digit_scan(const struct __lcat *, const char **s);

#endif
