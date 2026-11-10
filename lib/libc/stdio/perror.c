#include <string.h>
#include <errno.h>
#include "stdio_impl.h"

void perror(const char *msg)
{
	int saved = errno;
	const char *e = strerror(saved);
	FILE *f = stderr;
	FLOCK(f);
	if (msg && *msg) {
		fwrite(msg, 1, strlen(msg), f);
		fputc(':', f);
		fputc(' ', f);
	}
	fwrite(e, 1, strlen(e), f);
	fputc('\n', f);
	FUNLOCK(f);
	errno = saved;
}
