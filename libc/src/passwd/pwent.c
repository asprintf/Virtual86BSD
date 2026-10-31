#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "pwf.h"

static FILE *f;
static char *line;
static size_t n;

void setpwent(void) { if (f) rewind(f); }
void endpwent(void) { if (f) fclose(f); f = 0; }

struct passwd *getpwent(void)
{
	static struct passwd pw;
	int e = errno;
	if (!f && !(f = fopen(PASSWD_FILE, "re"))) {
		if (errno == ENOENT) errno = e;
		return 0;
	}
	for (;;) {
		if (getline(&line, &n, f) < 0) {
			if (!ferror(f)) errno = e;
			return 0;
		}
		if (!__parse_pw(line, &pw)) return &pw;
	}
}
