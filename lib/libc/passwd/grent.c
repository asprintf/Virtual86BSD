#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "pwf.h"

static FILE *f;
static char *line, **mem;
static size_t n, nmem;

void setgrent(void) { if (f) rewind(f); }
void endgrent(void) { if (f) fclose(f); f = 0; }

struct group *getgrent(void)
{
	static struct group gr;
	int e = errno, r;
	if (!f && !(f = fopen(GROUP_FILE, "re"))) {
		if (errno == ENOENT) errno = e;
		return 0;
	}
	for (;;) {
		if (getline(&line, &n, f) < 0) {
			if (!ferror(f)) errno = e;
			return 0;
		}
		if (!(r = __parse_gr(line, &gr, &mem, &nmem))) return &gr;
		if (r == -2) { errno = ENOMEM; return 0; }
	}
}
