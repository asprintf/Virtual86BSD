#include <time.h>
#include "time_impl.h"

char *__asctime_r(const struct tm *tm, char *buf);

char *asctime(const struct tm *tm)
{
	static char buf[64];
	return __asctime_r(tm, buf);
}

char *ctime(const time_t *t)
{
	static struct tm tm;
	struct tm *p = __localtime_r(t, &tm);
	return p ? asctime(p) : 0;
}
