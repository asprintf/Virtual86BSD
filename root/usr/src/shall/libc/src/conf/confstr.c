#include <unistd.h>
#include <string.h>
#include <errno.h>

size_t confstr(int name, char *buf, size_t len)
{
	const char *s;
	size_t l;
	switch (name) {
	case _CS_PATH: s = "/bin:/usr/bin"; break;
	case _CS_POSIX_V8_LP64_OFF64_CFLAGS: case _CS_POSIX_V7_LP64_OFF64_CFLAGS:
	case _CS_POSIX_V8_LP64_OFF64_LDFLAGS: case _CS_POSIX_V7_LP64_OFF64_LDFLAGS:
	case _CS_POSIX_V8_LP64_OFF64_LIBS: case _CS_POSIX_V7_LP64_OFF64_LIBS:
	case _CS_POSIX_V8_THREADS_CFLAGS: case _CS_POSIX_V7_THREADS_CFLAGS:
	case _CS_POSIX_V8_THREADS_LDFLAGS: case _CS_POSIX_V7_THREADS_LDFLAGS:
	case _CS_V8_ENV: case _CS_V7_ENV:
		s = ""; break;
	case _CS_POSIX_V8_WIDTH_RESTRICTED_ENVS: case _CS_POSIX_V7_WIDTH_RESTRICTED_ENVS:
		s = "POSIX_V8_LP64_OFF64"; break;
	default:
		if ((unsigned)name > _CS_V7_ENV) { errno = EINVAL; return 0; }
		/* the unsupported programming environments: no value */
		s = ""; break;
	}
	l = strlen(s);
	if (len) {
		size_t k = l < len ? l : len - 1;
		memcpy(buf, s, k);
		buf[k] = 0;
	}
	return l + 1;
}
