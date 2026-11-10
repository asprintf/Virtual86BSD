/* Differential: <pwd.h>, <grp.h> (the files /etc/passwd and /etc/group)
 * and scandir/alphasort/readdir on a few system directories. */
#define _XOPEN_SOURCE 800
#include <pwd.h>
#include <grp.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "out.h"

static void ppw(const struct passwd *p)
{
	if (!p) { os("none\n"); return; }
	os(p->pw_name); oc(':'); os(p->pw_passwd); oc(':'); ou(p->pw_uid); oc(':'); ou(p->pw_gid);
	oc(':'); os(p->pw_gecos); oc(':'); os(p->pw_dir); oc(':'); os(p->pw_shell); onl();
}

static void pgr(const struct group *g)
{
	char **m;
	if (!g) { os("none\n"); return; }
	os(g->gr_name); oc(':'); os(g->gr_passwd); oc(':'); ou(g->gr_gid); oc(':');
	for (m = g->gr_mem; *m; m++) { os(*m); if (m[1]) oc(','); }
	onl();
}

static int nodot(const struct dirent *d) { return d->d_name[0] != '.'; }

int main(void)
{
	struct passwd *p, pw, *pr;
	struct group *g, gr, *grr;
	struct dirent **v;
	char buf[4096], small[8];
	int i, n, r;
	unsigned long cnt = 0;
	static const char *dirs[] = { "/etc", "/usr/include", "/usr/bin", "/nonexistent", "/etc/passwd" };

	while ((p = getpwent())) ppw(p);
	setpwent();
	if ((p = getpwent())) ppw(p);
	endpwent();
	while ((g = getgrent())) pgr(g);
	endgrent();

	ppw(getpwnam("root")); ppw(getpwuid(0)); ppw(getpwnam("nosuchuser")); ppw(getpwuid(123456));
	pgr(getgrnam("root")); pgr(getgrgid(0)); pgr(getgrnam("nosuchgroup")); pgr(getgrgid(123456));
	r = getpwnam_r("root", &pw, buf, sizeof buf, &pr); oi(r); ppw(pr);
	r = getpwnam_r("root", &pw, small, sizeof small, &pr); oi(r == ERANGE); oi(pr == 0); onl();
	r = getpwuid_r(65534, &pw, buf, sizeof buf, &pr); oi(r); ppw(pr);
	r = getgrgid_r(0, &gr, buf + 1, sizeof buf - 1, &grr); oi(r); pgr(grr);
	r = getgrnam_r("root", &gr, small, sizeof small, &grr); oi(r == ERANGE); oi(grr == 0); onl();
	r = getgrnam_r("none-such", &gr, buf, sizeof buf, &grr); oi(r); oi(grr == 0); onl();
	for (i = 0; i < 2000; i++) {          /* by every id seen */
		p = getpwuid((uid_t)i); g = getgrgid((gid_t)i);
		if (p) { oi(i); oc(' '); os(p->pw_name); onl(); }
		if (g) { oi(i); oc(' '); os(g->gr_name); onl(); }
	}

	for (i = 0; i < 5; i++) {
		errno = 0;
		n = scandir(dirs[i], &v, i & 1 ? nodot : 0, alphasort);
		os(dirs[i]); oc(' '); oi(n); oc(' '); oi(n < 0 ? errno : 0); onl();
		for (r = 0; r < n; r++) {
			if (i != 2 || r < 50) { os(v[r]->d_name); onl(); }
			free(v[r]);
		}
		if (n >= 0) free(v);
	}
	{
		DIR *d = opendir("/usr/include");
		struct dirent *de;
		while ((de = readdir(d))) cnt += strlen(de->d_name);
		ou(cnt); onl();
		closedir(d);
	}
	oflush();
	return 0;
}
