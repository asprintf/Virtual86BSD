/* Minimal output helpers for tests: only write(), so the same test
 * program can be built against glibc (reference) and this libc. */
#include <unistd.h>
#include <string.h>

static char ob[1 << 16];
static size_t on;
static void oflush(void) { write(1, ob, on); on = 0; }
static void oc(char c) { if (on == sizeof ob) oflush(); ob[on++] = c; }
static void os(const char *s) { if (!s) s = "(null)"; while (*s) oc(*s++); }
static void ou(unsigned long long v)
{ char t[24]; int i = 0; do t[i++] = (char)('0' + v % 10); while (v /= 10); while (i) oc(t[--i]); }
static void oi(long long v) { if (v < 0) { oc('-'); ou(0ULL - (unsigned long long)v); } else ou((unsigned long long)v); }
static void ox(unsigned long long v)
{ static const char d[] = "0123456789abcdef"; char t[17]; int i = 0; do t[i++] = d[v & 15]; while (v >>= 4); while (i) oc(t[--i]); }
static void onl(void) { oc('\n'); }
/* sign of a comparison result */
static int sgn(long long v) { return (v > 0) - (v < 0); }

static unsigned long long rng_s = 88172645463325252ULL;
static unsigned long long rng(void)
{ rng_s ^= rng_s << 13; rng_s ^= rng_s >> 7; rng_s ^= rng_s << 17; return rng_s; }

static int failures;
#define CHECK(c) do { if (!(c)) { failures++; os("FAIL " __FILE__ ":"); oi(__LINE__); os(": " #c "\n"); } } while (0)
