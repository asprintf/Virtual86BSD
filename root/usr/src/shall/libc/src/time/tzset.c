#include <time.h>
#include "time_impl.h"

void tzset(void)
{
	__tz_lock();
	__tz_update();
	__tz_unlock();
}
