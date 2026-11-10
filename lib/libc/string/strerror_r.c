#include <string.h>
#include <errno.h>

const char *__strerror_msg(int);
char *__strerror_unknown(int, char *);

int strerror_r(int e, char *buf, size_t len)
{
	char tmp[32];
	const char *m = __strerror_msg(e);
	size_t l;
	int ret = 0;
	if (!m) { m = __strerror_unknown(e, tmp); ret = EINVAL; }
	l = strlen(m);
	if (l >= len) {
		if (len) { memcpy(buf, m, len - 1); buf[len - 1] = 0; }
		return ERANGE;
	}
	memcpy(buf, m, l + 1);
	return ret;
}
