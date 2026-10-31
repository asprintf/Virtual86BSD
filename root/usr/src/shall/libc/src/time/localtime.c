#include <time.h>
#include "time_impl.h"

struct tm *localtime(const time_t *t)
{
	static struct tm tm;
	return __localtime_r(t, &tm);
}

struct tm *gmtime(const time_t *t)
{
	static struct tm tm;
	return __gmtime_r(t, &tm);
}
