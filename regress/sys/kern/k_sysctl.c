/* k_sysctl.c - sysctl(3) and what the kernel answers (kern/kern_sysctl.c).
   As root, on V86BSD (the call is native).
 *
 * What is checked: the strings against uname(); the sizes (a NULL buffer
 * asks the size, a small one is ENOMEM, a host name is cut short but still
 * ends in NUL); setting the host and the domain name, and the limit on their
 * length; who may write (only root, and not a read-only value); the numbers
 * hw.* and kern.* make sense; the load average moves when a process runs;
 * the errors for a bad path, a bad pointer and a reserved id; sethostname()
 * and getloadavg() of the library. */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static int rd(int a, int b, void *buf, size_t *len)
{
	int mib[2] = { a, b };
	return sysctl(mib, 2, buf, len, NULL, 0);
}

static int rd_int(int a, int b, int *v)
{
	size_t len = sizeof *v;
	return rd(a, b, v, &len);
}

static int rd_str(int a, int b, char *buf, size_t size)
{
	size_t len = size;
	return rd(a, b, buf, &len);
}

static int wr_str(int a, int b, const char *s)
{
	int mib[2] = { a, b };
	return sysctl(mib, 2, NULL, NULL, (void *)s, strlen(s));
}

static void strings(void)
{
	struct utsname u;
	char buf[256];
	size_t len;
	uname(&u);
	CHECK(rd_str(CTL_KERN, KERN_OSTYPE, buf, sizeof buf) == 0 && strcmp(buf, u.sysname) == 0);
	CHECK(strcmp(buf, "V86BSD") == 0);
	CHECK(rd_str(CTL_KERN, KERN_OSRELEASE, buf, sizeof buf) == 0 && strcmp(buf, u.release) == 0);
	CHECK(rd_str(CTL_KERN, KERN_OSVERSION, buf, sizeof buf) == 0 && strcmp(buf, u.version) == 0);
	CHECK(rd_str(CTL_KERN, KERN_VERSION, buf, sizeof buf) == 0 && strncmp(buf, "Virtual/86 BSD ", 15) == 0);
	CHECK(rd_str(CTL_HW, HW_MACHINE, buf, sizeof buf) == 0 && strcmp(buf, "amd64") == 0 && strcmp(buf, u.machine) == 0);
	CHECK(rd_str(CTL_HW, HW_MODEL, buf, sizeof buf) == 0 && buf[0] != 0);

	/* the size alone, a buffer that is one short, one that fits */
	len = 0;
	CHECK(rd(CTL_KERN, KERN_OSTYPE, NULL, &len) == 0 && len == strlen("V86BSD") + 1);
	len = strlen("V86BSD");
	CHECK(rd(CTL_KERN, KERN_OSTYPE, buf, &len) == -1 && errno == ENOMEM);
	len = strlen("V86BSD") + 1;
	memset(buf, 'x', sizeof buf);
	CHECK(rd(CTL_KERN, KERN_OSTYPE, buf, &len) == 0 && len == 7 && strcmp(buf, "V86BSD") == 0 && buf[7] == 'x');
}

static void numbers(void)
{
	int v, i;
	size_t len;
	int64_t q;
	char names[256];
	CHECK(rd_int(CTL_HW, HW_NCPU, &v) == 0 && v == 1);
	CHECK(rd_int(CTL_HW, HW_NCPUONLINE, &v) == 0 && v == 1);
	CHECK(rd_int(CTL_HW, HW_BYTEORDER, &v) == 0 && v == 1234);
	CHECK(rd_int(CTL_HW, HW_PAGESIZE, &v) == 0 && v == sysconf(_SC_PAGESIZE));
	CHECK(rd_int(CTL_HW, HW_CPUSPEED, &v) == 0 && v > 100 && v < 100000);
	len = sizeof q;
	CHECK(rd(CTL_HW, HW_PHYSMEM64, &q, &len) == 0 && len == 8 && q >= 64 << 20 && (q & 4095) == 0);
	/* the 32-bit form is reserved, not answered (hw.physmem is the quad) */
	CHECK(rd_int(CTL_HW, HW_PHYSMEM, &v) == -1 && errno == EOPNOTSUPP);
	/* a quad does not fit an int's room */
	len = 4;
	CHECK(rd(CTL_HW, HW_PHYSMEM64, &q, &len) == -1 && errno == ENOMEM);
	CHECK(rd_int(CTL_HW, HW_DISKCOUNT, &v) == 0 && v >= 1);
	CHECK(rd_str(CTL_HW, HW_DISKNAMES, names, sizeof names) == 0 && strncmp(names, "disk0:", 6) == 0);
	for (i = 0, len = 0; names[i]; i++)
		if (names[i] == ':')
			len++;
	CHECK((int)len == v); /* one colon per disk */

	CHECK(rd_int(CTL_KERN, KERN_MAXPROC, &v) == 0 && v > 100);
	CHECK(rd_int(CTL_KERN, KERN_MAXFILES, &v) == 0 && v >= 1024);
	CHECK(rd_int(CTL_KERN, KERN_ARGMAX, &v) == 0 && v >= 131072);
	CHECK(rd_int(CTL_KERN, KERN_POSIX1, &v) == 0 && v == 202405);
	CHECK(rd_int(CTL_KERN, KERN_NGROUPS, &v) == 0 && v >= 16);
	CHECK(rd_int(CTL_KERN, KERN_JOB_CONTROL, &v) == 0 && v == 1);
	CHECK(rd_int(CTL_KERN, KERN_SAVED_IDS, &v) == 0 && v == 1);
	CHECK(rd_int(CTL_KERN, KERN_FSCALE, &v) == 0 && v == FSCALE);
	CHECK(rd_int(CTL_KERN, KERN_NPROCS, &v) == 0 && v >= 1);
	CHECK(rd_int(CTL_KERN, KERN_NTHREADS, &v) == 0 && v >= 1);

	{
		struct clockinfo ci;
		len = sizeof ci;
		CHECK(rd(CTL_KERN, KERN_CLOCKRATE, &ci, &len) == 0 && len == sizeof ci);
		CHECK(ci.hz == 1000 && ci.tick * ci.hz == 1000000 && ci.stathz > 0 && ci.profhz > 0);
	}
	{
		/* the boot time: in the past, and not long ago */
		struct { long sec, usec; } bt;
		time_t now = time(NULL);
		len = sizeof bt;
		CHECK(rd(CTL_KERN, KERN_BOOTTIME, &bt, &len) == 0 && len == sizeof bt);
		CHECK(bt.sec <= now && now - bt.sec < 24 * 3600 && bt.usec >= 0 && bt.usec < 1000000);
	}
}

static void names(void)
{
	char old[80], buf[80], big[80];
	int mib[2] = { CTL_KERN, KERN_HOSTNAME };
	size_t len;
	struct utsname u;

	CHECK(rd_str(CTL_KERN, KERN_HOSTNAME, old, sizeof old) == 0);
	{
		char g[80];
		CHECK(gethostname(g, sizeof g) == 0 && strcmp(g, old) == 0);
	}

	CHECK(wr_str(CTL_KERN, KERN_HOSTNAME, "k-sysctl-host") == 0);
	CHECK(rd_str(CTL_KERN, KERN_HOSTNAME, buf, sizeof buf) == 0 && strcmp(buf, "k-sysctl-host") == 0);
	uname(&u);
	CHECK(strcmp(u.nodename, "k-sysctl-host") == 0); /* uname and sethostname see the same name */

	/* old and new in one call: the old value comes back, the new one is set */
	len = sizeof buf;
	CHECK(sysctl(mib, 2, buf, &len, "second", 6) == 0 && strcmp(buf, "k-sysctl-host") == 0 && len == 14);
	CHECK(rd_str(CTL_KERN, KERN_HOSTNAME, buf, sizeof buf) == 0 && strcmp(buf, "second") == 0);

	/* a small buffer gets the name cut short, with its NUL */
	memset(buf, 'x', sizeof buf);
	len = 4;
	CHECK(sysctl(mib, 2, buf, &len, NULL, 0) == 0 && len == 4 && strcmp(buf, "sec") == 0 && buf[4] == 'x');
	len = 0;
	CHECK(sysctl(mib, 2, buf, &len, NULL, 0) == -1 && errno == ENOMEM);

	/* 64 characters fit (the name is a 65-byte buffer with its NUL), 65 do not */
	memset(big, 'h', 64);
	big[64] = 0;
	CHECK(wr_str(CTL_KERN, KERN_HOSTNAME, big) == 0);
	CHECK(rd_str(CTL_KERN, KERN_HOSTNAME, buf, sizeof buf) == 0 && strlen(buf) == 64);
	big[64] = 'h';
	big[65] = 0;
	CHECK(wr_str(CTL_KERN, KERN_HOSTNAME, big) == -1 && errno == EINVAL);
	CHECK(rd_str(CTL_KERN, KERN_HOSTNAME, buf, sizeof buf) == 0 && strlen(buf) == 64); /* unchanged */
	CHECK(wr_str(CTL_KERN, KERN_HOSTNAME, "") == 0);
	CHECK(rd_str(CTL_KERN, KERN_HOSTNAME, buf, sizeof buf) == 0 && buf[0] == 0);

	/* the library's sethostname */
	CHECK(sethostname("libc-name", 9) == 0);
	CHECK(rd_str(CTL_KERN, KERN_HOSTNAME, buf, sizeof buf) == 0 && strcmp(buf, "libc-name") == 0);
	CHECK(sethostname(big, 65) == -1 && errno == EINVAL);

	/* the domain name is the same kind of value */
	{
		char dom[80];
		CHECK(rd_str(CTL_KERN, KERN_DOMAINNAME, dom, sizeof dom) == 0);
		CHECK(wr_str(CTL_KERN, KERN_DOMAINNAME, "k.example") == 0);
		CHECK(rd_str(CTL_KERN, KERN_DOMAINNAME, buf, sizeof buf) == 0 && strcmp(buf, "k.example") == 0);
		uname(&u);
		CHECK(strcmp(u.__domainname, "k.example") == 0);
		CHECK(wr_str(CTL_KERN, KERN_DOMAINNAME, dom) == 0);
	}
	CHECK(wr_str(CTL_KERN, KERN_HOSTNAME, old) == 0);
}

/* the errors, and who may write */
static void errors(void)
{
	int mib[CTL_MAXNAME] = { CTL_KERN, KERN_OSTYPE, 1 };
	char buf[64];
	size_t len = sizeof buf;
	pid_t pid;
	int st;

	CHECK(sysctl(mib, 0, buf, &len, NULL, 0) == -1 && errno == EINVAL);
	CHECK(sysctl(mib, 1, buf, &len, NULL, 0) == -1 && errno == EINVAL); /* a top-level name is a node */
	CHECK(sysctl(mib, CTL_MAXNAME + 1, buf, &len, NULL, 0) == -1 && errno == EINVAL);
	len = sizeof buf;
	CHECK(sysctl(mib, 3, buf, &len, NULL, 0) == -1 && errno == ENOTDIR); /* kern.ostype.1 */

	mib[0] = CTL_DEBUG; mib[1] = 0;
	CHECK(sysctl(mib, 2, buf, &len, NULL, 0) == -1 && errno == EOPNOTSUPP);
	mib[0] = CTL_NET; mib[1] = 2; /* net.inet */
	CHECK(sysctl(mib, 2, buf, &len, NULL, 0) == -1 && errno == EOPNOTSUPP);
	mib[0] = 99; mib[1] = 1;
	CHECK(sysctl(mib, 2, buf, &len, NULL, 0) == -1 && errno == EOPNOTSUPP);
	CHECK(rd_int(CTL_KERN, KERN_PROC, &st) == -1 && errno == EOPNOTSUPP); /* reserved */
	CHECK(rd_int(CTL_KERN, 5000, &st) == -1 && errno == EOPNOTSUPP);
	CHECK(rd_int(CTL_KERN, -1, &st) == -1 && errno == EOPNOTSUPP);
	CHECK(rd_int(CTL_HW, 5000, &st) == -1 && errno == EOPNOTSUPP);
	CHECK(rd_int(CTL_VM, VM_METER, &st) == -1 && errno == EOPNOTSUPP);

	/* bad pointers */
	mib[0] = CTL_KERN; mib[1] = KERN_OSTYPE;
	len = sizeof buf;
	CHECK(sysctl(mib, 2, (void *)8, &len, NULL, 0) == -1 && errno == EFAULT);
	CHECK(sysctl((int *)8, 2, buf, &len, NULL, 0) == -1 && errno == EFAULT);
	CHECK(sysctl(mib, 2, buf, (size_t *)8, NULL, 0) == -1 && errno == EFAULT);
	CHECK(sysctl(mib, 2, buf, NULL, NULL, 0) == -1 && errno == EINVAL); /* a buffer needs its size */
	mib[1] = KERN_HOSTNAME;
	CHECK(sysctl(mib, 2, NULL, NULL, (void *)8, 4) == -1 && errno == EFAULT);

	/* a value that cannot be written, as root */
	mib[1] = KERN_OSTYPE;
	CHECK(sysctl(mib, 2, NULL, NULL, "x", 1) == -1 && errno == EPERM);
	CHECK(rd_str(CTL_KERN, KERN_OSTYPE, buf, sizeof buf) == 0 && strcmp(buf, "V86BSD") == 0);
	len = sizeof(int);
	{
		int v = 5;
		mib[1] = KERN_MAXPROC;
		CHECK(sysctl(mib, 2, NULL, NULL, &v, sizeof v) == -1 && errno == EPERM);
	}

	/* not root: reading works, writing is EPERM (even for the host name) */
	pid = fork();
	if (pid == 0) {
		char h[80];
		int bad = 0;
		if (setgid(65534) || setuid(65534))
			_exit(2);
		if (rd_str(CTL_KERN, KERN_HOSTNAME, h, sizeof h) != 0)
			bad |= 1;
		if (wr_str(CTL_KERN, KERN_HOSTNAME, "evil") != -1 || errno != EPERM)
			bad |= 2;
		if (sethostname("evil", 4) != -1 || errno != EPERM)
			bad |= 4;
		if (rd_str(CTL_KERN, KERN_HOSTNAME, h, sizeof h) != 0 || strcmp(h, "evil") == 0)
			bad |= 8;
		_exit(bad);
	}
	CHECK(pid > 0 && waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
	if (WIFEXITED(st) && WEXITSTATUS(st))
		printf("  child (not root) status %d\n", WEXITSTATUS(st));
}

/* a process that runs raises the load average (it is sampled every 5 s) */
static void loadavg(void)
{
	struct loadavg la;
	size_t len = sizeof la;
	double d[4] = { -1, -1, -1, -1 };
	pid_t kid[2];
	int i, n, st;
	unsigned before;

	CHECK(rd(CTL_VM, VM_LOADAVG, &la, &len) == 0 && len == sizeof la && la.fscale == FSCALE);
	before = la.ldavg[0];
	CHECK(getloadavg(d, 3) == 3 && d[0] >= 0 && d[1] >= 0 && d[2] >= 0);
	CHECK(getloadavg(d, 0) == 0);
	CHECK(getloadavg(d, 1) == 1);
	CHECK(getloadavg(d, 9) == 3);
	CHECK(getloadavg(d, -1) == -1 && errno == EINVAL);

	for (i = 0; i < 2; i++) {
		kid[i] = fork();
		if (kid[i] == 0) {
			volatile unsigned long x = 0;
			for (;;)
				x++;
		}
	}
	sleep(12); /* two samples */
	CHECK(rd(CTL_VM, VM_LOADAVG, &la, &len) == 0);
	CHECK(la.ldavg[0] > before);
	n = (int)(la.ldavg[0] * 100 / FSCALE);
	CHECK(n > 0);
	CHECK(la.ldavg[0] >= la.ldavg[1] && la.ldavg[1] >= la.ldavg[2]); /* the shorter average rises faster */
	CHECK(getloadavg(d, 3) == 3 && d[0] > 0 && d[0] < 4);
	for (i = 0; i < 2; i++) {
		kill(kid[i], SIGKILL);
		waitpid(kid[i], &st, 0);
	}
	printf("  loadavg after 12 s of two busy processes: %.2f %.2f %.2f\n", d[0], d[1], d[2]);
}

int main(void)
{
	if (geteuid() != 0) {
		printf("k_sysctl: run as root\n");
		return 1;
	}
	strings();
	numbers();
	names();
	errors();
	loadavg();
	if (failures) {
		printf("k_sysctl: %d FAILED\n", failures);
		return 1;
	}
	printf("k_sysctl: ok\n");
	return 0;
}
