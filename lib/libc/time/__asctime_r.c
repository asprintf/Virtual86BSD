/* The format of asctime(): "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n". */
#include <time.h>
#include <string.h>

static char *num(char *p, long v, int width, char pad)
{
	char t[24];
	int n = 0, neg = v < 0;
	unsigned long u = neg ? 0UL - (unsigned long)v : (unsigned long)v;
	do t[n++] = (char)('0' + u % 10); while (u /= 10);
	if (neg) t[n++] = '-';
	while (n < width) t[n++] = pad;
	while (n) *p++ = t[--n];
	return p;
}

char *__asctime_r(const struct tm *tm, char *buf)
{
	static const char wday[] = "SunMonTueWedThuFriSat";
	static const char mon[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
	char *p = buf;
	if ((unsigned)tm->tm_wday < 7) memcpy(p, wday + 3 * tm->tm_wday, 3); else memcpy(p, "???", 3);
	p += 3; *p++ = ' ';
	if ((unsigned)tm->tm_mon < 12) memcpy(p, mon + 3 * tm->tm_mon, 3); else memcpy(p, "???", 3);
	p += 3;
	p = num(p, tm->tm_mday, 3, ' '); *p++ = ' ';
	p = num(p, tm->tm_hour, 2, '0'); *p++ = ':';
	p = num(p, tm->tm_min, 2, '0'); *p++ = ':';
	p = num(p, tm->tm_sec, 2, '0'); *p++ = ' ';
	p = num(p, 1900L + tm->tm_year, 0, ' ');
	*p++ = '\n'; *p = 0;
	return buf;
}
