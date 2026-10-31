#include <string.h>
#include <errno.h>

static const char *const msgs[] = {
	[0] = "Success",
#define E(n, sym, msg) [n] = msg,
#include "../errno/errlist.h"
#undef E
};
#define NMSGS (sizeof msgs / sizeof *msgs)

/* Returns 0 for an unknown error number. */
const char *__strerror_msg(int e)
{
	if (e < 0 || (unsigned)e >= NMSGS || !msgs[e]) return 0;
	return msgs[e];
}

char *__strerror_unknown(int e, char *buf)
{
	/* "Unknown error -2147483648" fits in 32 bytes. */
	char tmp[16], *p = tmp + sizeof tmp;
	unsigned u = e < 0 ? 0U - (unsigned)e : (unsigned)e;
	*--p = 0;
	do *--p = (char)('0' + u % 10); while (u /= 10);
	if (e < 0) *--p = '-';
	memcpy(buf, "Unknown error ", 14);
	strcpy(buf + 14, p);
	return buf;
}

char *strerror(int e)
{
	static char buf[32]; /* only for unknown numbers */
	const char *m = __strerror_msg(e);
	return m ? (char *)m : __strerror_unknown(e, buf);
}

