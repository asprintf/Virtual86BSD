/* Signal sets.  The library reserves signals 32-34 for itself; they can
 * be neither added to a set nor are they part of a filled set. */
#include <signal.h>
#include <string.h>
#include <errno.h>

#define RESERVED(s) ((unsigned)(s) - 32 < 3)
#define BAD(s) ((unsigned)(s) - 1 >= _NSIG - 1 || RESERVED(s))

int sigemptyset(sigset_t *set) { memset(set, 0, sizeof *set); return 0; }

int sigfillset(sigset_t *set)
{
	memset(set, 0, sizeof *set);
	set->__bits[0] = ~(7UL << 31); /* 1-31 and 35-64; not 32-34 */
	return 0;
}

int sigaddset(sigset_t *set, int s)
{
	if (BAD(s)) { errno = EINVAL; return -1; }
	set->__bits[0] |= 1UL << (s - 1);
	return 0;
}

int sigdelset(sigset_t *set, int s)
{
	if (BAD(s)) { errno = EINVAL; return -1; }
	set->__bits[0] &= ~(1UL << (s - 1));
	return 0;
}

int sigismember(const sigset_t *set, int s)
{
	if ((unsigned)s - 1 >= _NSIG - 1) { errno = EINVAL; return -1; }
	return (int)(set->__bits[0] >> (s - 1) & 1);
}
