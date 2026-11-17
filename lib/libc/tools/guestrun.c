/* guestrun.c - /sbin/init of the gate image (gate.sh): runs the tests of /g/list,
 * one per line "ID DIR PROG TIMEOUT", with DIR as the directory, no standard
 * input, and prints each one's output (at most 200000 bytes) between marks:
 *   @@@ BEGIN id
 *   ...output...
 *   @@@ END id status S
 * S is the wait status, or 9999 when the test ran past its time.  Variables of
 * /g/env are set.  Then the machine is powered off. */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/sysctl.h>
#include <sys/reboot.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define CAP 200000

static void emit(const char *s, size_t n)
{
	while (n) {
		ssize_t w = write(1, s, n);
		if (w < 0) { if (errno == EINTR) continue; return; }
		s += w; n -= (size_t)w;
	}
}

static void rootrw(void)
{
	struct iovec iov[10];
	int i = 0;
#define NV(n, v, l) do { iov[i].iov_base = (void *)(n); iov[i++].iov_len = strlen(n) + 1; \
	iov[i].iov_base = (void *)(v); iov[i++].iov_len = (l); } while (0)
	NV("fstype", "ufs", 4);
	NV("fspath", "/", 2);
	NV("from", "/dev/gpt/rootfs", 16);
	NV("update", 0, 0);
	NV("noro", 0, 0);
	if (nmount(iov, 10, MNT_UPDATE) < 0) printf("@@@ rootrw: nmount failed: %d\n", errno);
}

static void ifup(const char *name, unsigned addr, unsigned mask, unsigned bcast)
{
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	struct ifaliasreq a;
	struct ifreq r;
	struct sockaddr_in *sin;
	if (s < 0) return;
	memset(&a, 0, sizeof a);
	strcpy(a.ifra_name, name);
	sin = (struct sockaddr_in *)&a.ifra_addr;
	sin->sin_len = sizeof *sin; sin->sin_family = AF_INET; sin->sin_addr.s_addr = htonl(addr);
	sin = (struct sockaddr_in *)&a.ifra_mask;
	sin->sin_len = sizeof *sin; sin->sin_family = AF_INET; sin->sin_addr.s_addr = htonl(mask);
	sin = (struct sockaddr_in *)&a.ifra_broadaddr;
	if (bcast) { sin->sin_len = sizeof *sin; sin->sin_family = AF_INET; sin->sin_addr.s_addr = htonl(bcast); }
	if (ioctl(s, SIOCAIFADDR, &a) < 0) printf("@@@ %s: SIOCAIFADDR failed: %d\n", name, errno);
	memset(&r, 0, sizeof r);
	strcpy(r.ifr_name, name);
	if (ioctl(s, SIOCGIFFLAGS, &r) == 0) {
		r.ifr_flags |= IFF_UP;
		if (ioctl(s, SIOCSIFFLAGS, &r) < 0) printf("@@@ %s: SIOCSIFFLAGS failed: %d\n", name, errno);
	}
	close(s);
}

/* FreeBSD's struct rt_msghdr (sys/net/route.h); libc has no <net/route.h> for it */
struct rtmsg { unsigned short len; unsigned char version, type; unsigned short index, spare; int flags, addrs, pid, seq, err, fmask;
	unsigned long inits, rmx[14]; };
#define RTM_ADD 1
#define RTM_VERSION 5
#define RTF_UP 1
#define RTF_GATEWAY 2
#define RTF_STATIC 0x800
#define RTA_DST 1
#define RTA_GATEWAY 2
#define RTA_NETMASK 4

static void defroute(unsigned gw)
{
	struct { struct rtmsg h; struct sockaddr_in a[3]; } m;
	int i, s = socket(PF_ROUTE, SOCK_RAW, 0);
	if (s < 0) return;
	memset(&m, 0, sizeof m);
	m.h.len = sizeof m; m.h.version = RTM_VERSION; m.h.type = RTM_ADD;
	m.h.flags = RTF_UP | RTF_GATEWAY | RTF_STATIC; m.h.addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
	m.h.seq = 1;
	for (i = 0; i < 3; i++) { m.a[i].sin_len = sizeof m.a[i]; m.a[i].sin_family = AF_INET; }
	m.a[1].sin_addr.s_addr = htonl(gw);
	if (write(s, &m, sizeof m) < 0) printf("@@@ default route failed: %d\n", errno);
	close(s);
}

int main(void)
{
	int fd = open("/dev/console", O_RDWR), st;
	FILE *f;
	char line[1024];
	if (fd < 0) _exit(1);
	dup2(fd, 0); dup2(fd, 1); dup2(fd, 2);
	if (fd > 2) close(fd);
	setsid();
	sethostname("vm", 2);
	{
		struct utsname u;
		if (uname(&u) == 0) printf("@@@ UNAME %s %s %s\n", u.sysname, u.release, u.machine);
	}
	printf("@@@ INIT pid %d\n", (int)getpid());
	fflush(stdout);
	if ((f = fopen("/g/env", "r"))) {
		while (fgets(line, sizeof line, f)) {
			line[strcspn(line, "\n")] = 0;
			if (strchr(line, '=')) putenv(strdup(line));
		}
		fclose(f);
	}
	rootrw();
	{
		int zero = 0, rfd = open("/dev/random", O_RDONLY);
		char c;
		int one = 1;
		sysctlbyname("kern.logsigexit", 0, 0, &zero, sizeof zero);
		sysctlbyname("net.inet.ip.connect_inaddr_wild", 0, 0, &one, sizeof one);
		if (rfd >= 0) { if (read(rfd, &c, 1) < 0) {} close(rfd); }   /* waits for the "random: unblocking device." message */
	}
	ifup("lo0", 0x7f000001, 0xff000000, 0);
	ifup("vtnet0", 0x0a00020f, 0xffffff00, 0x0a0002ff);
	defroute(0x0a000202);
	f = fopen("/g/list", "r");
	if (!f) printf("@@@ NOLIST\n");
	while (f && fgets(line, sizeof line, f)) {
		char *id, *dir, *prog, *tmo;
		static char buf[CAP + 4096];
		int p[2];
		pid_t pid;
		size_t len = 0;
		int tmoms, timedout = 0, exited;
		struct timespec t0, t;
		line[strcspn(line, "\n")] = 0;
		id = strtok(line, " ");
		dir = strtok(0, " ");
		prog = strtok(0, " ");
		tmo = strtok(0, " ");
		if (!id || !dir || !prog) continue;
		tmoms = (tmo ? atoi(tmo) : 60) * 1000;
		printf("@@@ BEGIN %s\n", id);
		fflush(stdout);
		if (pipe(p) < 0) { printf("pipe failed\n"); continue; }
		if ((pid = fork()) == 0) {
			int z = open("/dev/null", O_RDONLY);
			char *av[2];
			setpgid(0, 0);
			close(p[0]);
			if (z >= 0) dup2(z, 0);
			dup2(p[1], 1); dup2(p[1], 2);
			if (p[1] > 2) close(p[1]);
			if (chdir(dir) < 0) { printf("chdir %s failed: %d\n", dir, errno); _exit(127); }
			av[0] = prog; av[1] = 0;
			execv(prog, av);
			printf("exec %s failed: %d\n", prog, errno);
			_exit(127);
		}
		close(p[1]);
		clock_gettime(CLOCK_MONOTONIC, &t0);
		exited = 0;
		for (;;) {
			struct pollfd pf = { p[0], POLLIN, 0 };
			long left;
			int r;
			clock_gettime(CLOCK_MONOTONIC, &t);
			left = tmoms - ((long)(t.tv_sec - t0.tv_sec) * 1000 + (t.tv_nsec - t0.tv_nsec) / 1000000);
			if (left <= 0) { timedout = 1; break; }
			r = poll(&pf, 1, (int)(left > 100 ? 100 : left));
			if (r < 0 && errno == EINTR) continue;
			if (r == 0) {	/* the test's process may be gone while a child of it holds the pipe */
				if (waitpid(pid, &st, WNOHANG) == pid) { exited = 1; break; }
				continue;
			}
			{
				char b[4096];
				ssize_t n = read(p[0], b, sizeof b);
				if (n <= 0) break;
				if (len < CAP) {
					size_t c = (size_t)n < CAP - len ? (size_t)n : CAP - len;
					memcpy(buf + len, b, c);
					len += c;
				}
			}
		}
		if (exited) {	/* what is left in the pipe */
			char b[4096];
			ssize_t n;
			fcntl(p[0], F_SETFL, O_NONBLOCK);
			while ((n = read(p[0], b, sizeof b)) > 0 && len < CAP) {
				size_t c = (size_t)n < CAP - len ? (size_t)n : CAP - len;
				memcpy(buf + len, b, c);
				len += c;
			}
		}
		if (timedout) { kill(-pid, SIGKILL); kill(pid, SIGKILL); }
		close(p[0]);
		if (!exited)
			while (waitpid(pid, &st, 0) < 0 && errno == EINTR);
		kill(-1, SIGKILL);
		while (waitpid(-1, 0, WNOHANG) > 0);
		emit(buf, len);
		printf("\n@@@ END %s status %d\n", id, timedout ? 9999 : st);
		fflush(stdout);
	}
	printf("@@@ DONE\n");
	fflush(stdout);
	ioctl(1, TIOCDRAIN);
	sync();
	reboot(RB_POWEROFF);
	return 0;
}
