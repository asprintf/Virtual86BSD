/* An application may define ftw, ioctl, sethostname and vsyslog itself: they
 * are not POSIX names, so libc must neither collide with them nor call them. */
#define _DEFAULT_SOURCE
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <ftw.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include "out.h"

static int calls;

void vsyslog(int pri, const char *fmt, va_list ap) { (void)pri; (void)fmt; (void)ap; calls++; }
int sethostname(const char *name, size_t len) { (void)name; (void)len; calls++; return -1; }
int ftw(const char *path, int (*fn)(const char *, const struct stat *, int), int nd)
{ (void)path; (void)fn; (void)nd; calls++; return -1; }
int ioctl(int fd, unsigned long req, ...) { (void)fd; (void)req; calls++; return -1; }

static int visits;
static int visit(const char *p, const struct stat *st, int t, struct FTW *w)
{ (void)p; (void)st; (void)t; (void)w; visits++; return 0; }

int main(void)
{
	struct utsname u;
	struct if_nameindex *ni;
	struct ifaddrs *ifa;
	char name[IF_NAMESIZE];

	syslog(LOG_INFO, "nsclash");
	CHECK(calls == 0);
	CHECK(uname(&u) == 0 && calls == 0);
	CHECK(nftw(".", visit, 4, FTW_PHYS) == 0 && visits > 0 && calls == 0);
	ni = if_nameindex();
	CHECK(ni && ni[0].if_index && calls == 0);
	if (ni) {
		CHECK(if_nametoindex(ni[0].if_name) == ni[0].if_index);
		CHECK(if_indextoname(ni[0].if_index, name) && !strcmp(name, ni[0].if_name));
		if_freenameindex(ni);
	}
	CHECK(getifaddrs(&ifa) == 0 && calls == 0);
	if (ifa)
		freeifaddrs(ifa);
	CHECK(calls == 0);
	os(failures ? "nsclash FAILED\n" : "nsclash ok\n");
	oflush();
	return failures != 0;
}
