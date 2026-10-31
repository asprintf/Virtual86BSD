#include <unistd.h>
#include <errno.h>
#include "syscall.h"

struct ktimespec { long tv_sec, tv_nsec; };

unsigned sleep(unsigned sec)
{
	struct ktimespec ts = { (long)sec, 0 };
	if (__syscall_cp2(SYS_nanosleep, &ts, &ts) == -EINTR)
		return (unsigned)ts.tv_sec + (ts.tv_nsec > 0);
	return 0;
}

unsigned alarm(unsigned sec) { return (unsigned)__syscall1(SYS_alarm, sec); }
int pause(void) { return (int)__sys_cp4(SYS_ppoll, 0, 0, 0, 0); }
