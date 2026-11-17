/* gateparse.c - on the host: split the serial log of a gate.sh run (marks of guestrun.c).
 *   gateparse ost LOG OUTDIR   OUTDIR/ID.out like os-test's ost.sh makes them
 *   gateparse pts LOG RESULTS  lines "ID STATUS" appended to RESULTS
 * Prints the number of finished tests and, on stderr, the last test that began and did not end. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

static void mkpath(char *p)
{
	char *s;
	for (s = p + 1; (s = strchr(s, '/')); *s++ = '/') { *s = 0; mkdir(p, 0777); }
}

int main(int argc, char **argv)
{
	FILE *f;
	char *buf, *p;
	long n, done = 0;
	int ost;
	char last[1024] = "";
	FILE *res = 0, *ids;
	char idp[2048];
	if (argc != 4) return 2;
	ost = !strcmp(argv[1], "ost");
	if (!(f = fopen(argv[2], "rb"))) { perror(argv[2]); return 1; }
	fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
	buf = malloc(n + 1);
	if (fread(buf, 1, n, f) != (size_t)n) return 1;
	buf[n] = 0; fclose(f);
	/* \r\n -> \n */
	{
		char *w = buf;
		for (p = buf; *p; p++) if (!(*p == '\r' && p[1] == '\n')) *w++ = *p;
		*w = 0; n = w - buf;
	}
	if (!ost) res = fopen(argv[3], "a");
	snprintf(idp, sizeof idp, "%s.ids", argv[3]);
	ids = fopen(idp, "a");
	for (p = buf; (p = strstr(p, "@@@ BEGIN ")); ) {
		char id[1024], path[2048];
		char *nl = strchr(p, '\n'), *e, *body, *endl;
		int st = 0;
		size_t blen;
		if (!nl) break;
		sscanf(p + 10, "%1023s", id);
		strcpy(last, id);
		body = nl + 1;
		/* the END mark of this id, after a newline */
		snprintf(path, sizeof path, "\n@@@ END %s status ", id);
		e = strstr(body, path);
		if (!e) break;
		blen = e - body;
		endl = e + strlen(path);
		st = atoi(endl);
		last[0] = 0;
		{
			int code = (st == 9999) ? 9999 : ((st & 0x7f) == 0 ? (st >> 8) & 0xff : 128 + (st & 0x7f));
			if (ost) {
				FILE *o;
				snprintf(path, sizeof path, "%s/%s.out", argv[3], id);
				mkpath(path);
				o = fopen(path, "wb");
				if (!o) { perror(path); return 1; }
				fwrite(body, 1, blen, o);
				if (blen == 0 || code >= 2) fprintf(o, "exit: %d\n", code == 9999 ? 137 : code);
				fclose(o);
			} else {
				const char *s;
				switch (code) {
				case 0: s = "PASS"; break;
				case 1: s = "FAIL"; break;
				case 2: s = "UNRESOLVED"; break;
				case 4: s = "UNSUPPORTED"; break;
				case 5: s = "UNTESTED"; break;
				case 9999: s = "HUNG"; break;
				default: s = 0;
				}
				if (s) fprintf(res, "%s %s\n", id, s);
				else if (code > 128) fprintf(res, "%s SIGNAL-%d\n", id, code - 128);
				else fprintf(res, "%s EXIT-%d\n", id, code);
			}
		}
		fprintf(ids, "%s\n", id);
		done++;
		p = endl;
	}
	if (res) fclose(res);
	fclose(ids);
	if ((p = strstr(buf, "@@@ UNAME "))) {
		char u[512];
		if (sscanf(p + 10, "%511[^\n]", u) == 1) fprintf(stderr, "UNAME %s\n", u);
	}
	printf("%ld\n", done);
	if (last[0]) fprintf(stderr, "UNFINISHED %s\n", last);
	return 0;
}
