/* vminit.c - /sbin/init of the libc test image (vmrun.sh): runs the programs
 * listed in /t/list, one per line as NAME[ STDIN[ ARG]] (STDIN a file in /t or -),
 * with the variables of /t/env, and prints each one's output (at most the
 * first 6000 bytes and the lines with FAIL) between marks:
 *   @@@ BEGIN name
 *   ...output...
 *   @@@ END name status N len L hash H
 * and, for a long output, "@@@ BLK n hash" lines (the hash of each 200 lines);
 * /t/dump "name first count" adds those lines to the output in full.
 * L and H (FNV-1a, 64 bits) are of the whole output, which the serial line
 * could not carry (some tests write megabytes); then it powers the machine off. */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static void emit(const char *s, size_t n)
{
	while (n) {
		ssize_t w = write(1, s, n);
		if (w < 0) { if (errno == EINTR) continue; return; }
		s += w; n -= (size_t)w;
	}
}

/* the kernel mounts the root read-only: mount -u -w / */
/* a test past its time: kill whatever still holds its output (children that left its group too) */
static void on_timeout(int s)
{
	(void)s;
	kill(-1, SIGKILL);
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

/* lo0 is down in a new system: 127.0.0.1/8 on it, up (what ifconfig lo0 inet 127.0.0.1 does) */
static void lo0up(void)
{
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	struct ifaliasreq a;
	struct ifreq r;
	struct sockaddr_in *sin;
	if (s < 0) return;
	memset(&a, 0, sizeof a);
	strcpy(a.ifra_name, "lo0");
	sin = (struct sockaddr_in *)&a.ifra_addr;
	sin->sin_len = sizeof *sin; sin->sin_family = AF_INET; sin->sin_addr.s_addr = htonl(0x7f000001);
	sin = (struct sockaddr_in *)&a.ifra_mask;
	sin->sin_len = sizeof *sin; sin->sin_family = AF_INET; sin->sin_addr.s_addr = htonl(0xff000000);
	if (ioctl(s, SIOCAIFADDR, &a) < 0) printf("@@@ lo0: SIOCAIFADDR failed: %d\n", errno);
	memset(&r, 0, sizeof r);
	strcpy(r.ifr_name, "lo0");
	if (ioctl(s, SIOCGIFFLAGS, &r) == 0) {
		r.ifr_flags |= IFF_UP;
		if (ioctl(s, SIOCSIFFLAGS, &r) < 0) printf("@@@ lo0: SIOCSIFFLAGS failed: %d\n", errno);
	}
	close(s);
}

int main(void)
{
	struct sigaction sa;
	struct timespec t0, t1;
	int fd = open("/dev/console", O_RDWR), st;
	FILE *f;
	char line[512];
	if (fd < 0) _exit(1);
	dup2(fd, 0); dup2(fd, 1); dup2(fd, 2);
	if (fd > 2) close(fd);
	setsid();                      /* as FreeBSD's init: the kernel's session 0 is no session */
	sethostname("vm", 2);          /* unset, the kernel's host name is empty */
	printf("@@@ INIT pid %d\n", (int)getpid());
	fflush(stdout);
	if ((f = fopen("/t/env", "r"))) {
		while (fgets(line, sizeof line, f)) {
			line[strcspn(line, "\n")] = 0;
			if (strchr(line, '=')) putenv(strdup(line));
		}
		fclose(f);
	}
	rootrw();
	lo0up();
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_timeout;
	sigaction(SIGALRM, &sa, 0);
	chdir("/t");
	f = fopen("/t/list", "r");
	if (!f) printf("@@@ NOLIST\n");
	while (f && fgets(line, sizeof line, f)) {
		char *name = line, *in, *arg, path[300];
		int p[2];
		pid_t pid;
		line[strcspn(line, "\n")] = 0;
		if (!*line) continue;
		in = strchr(line, ' ');
		arg = 0;
		if (in) {
			*in++ = 0;
			if ((arg = strchr(in, ' '))) *arg++ = 0;
		}
		snprintf(path, sizeof path, "/t/%s", name);
		printf("@@@ BEGIN %s\n", name);
		fflush(stdout);
		if (pipe(p) < 0) { printf("pipe failed\n"); continue; }
		if ((pid = fork()) == 0) {
			char *av[3];
			int i = 0;
			close(p[0]);
			dup2(p[1], 1); dup2(p[1], 2);
			if (p[1] > 2) close(p[1]);
			if (in && strcmp(in, "-")) {
				int s = open(in, O_RDONLY);
				if (s >= 0) { dup2(s, 0); close(s); }
			}
			av[i++] = path;
			if (arg) av[i++] = arg;
			av[i] = 0;
			execv(path, av);
			printf("exec %s failed: %d\n", path, errno);
			_exit(127);
		}
		close(p[1]);
		clock_gettime(CLOCK_MONOTONIC, &t0);
		alarm(100);
		{
			unsigned long long h = 14695981039346656037ULL, len = 0, bh = 14695981039346656037ULL;
			unsigned long line = 0, nblk = 0, bhs[4096];
			char lbuf[1024], dname[100] = "";
			unsigned long dfirst = 0, dcount = 0;
			size_t ll = 0;
			{
				FILE *df = fopen("/t/dump", "r");
				if (df) { if (fscanf(df, "%99s %lu %lu", dname, &dfirst, &dcount) != 3) dname[0] = 0; fclose(df); }
			}
			int dump = !strcmp(dname, name);
			for (;;) {
				char buf[4096];
				ssize_t n = read(p[0], buf, sizeof buf);
				ssize_t i;
				if (n < 0 && errno == EINTR) continue;
				if (n <= 0) break;
				for (i = 0; i < n; i++) {
					h = (h ^ (unsigned char)buf[i]) * 1099511628211ULL;
					bh = (bh ^ (unsigned char)buf[i]) * 1099511628211ULL;
					if (buf[i] == '\n') {
						line++;
						if (line % 200 == 0 && nblk < 4096) { bhs[nblk++] = bh; bh = 14695981039346656037ULL; }
					}
					if (dump && line >= dfirst && line < dfirst + dcount) emit(buf + i, 1);
					if (len + (unsigned long long)i < 6000) emit(buf + i, 1);
					else if (ll < sizeof lbuf - 1) {
						lbuf[ll++] = buf[i];
						if (buf[i] == '\n') {
							lbuf[ll] = 0;
							if (strstr(lbuf, "FAIL")) emit(lbuf, ll);
							ll = 0;
						}
					} else ll = 0;
				}
				len += (unsigned long long)n;
			}
			close(p[0]);
			while (waitpid(pid, &st, 0) < 0 && errno == EINTR);
			alarm(0);
			{
				unsigned long k;
				if (len > 6000) for (k = 0; k < nblk; k++) printf("@@@ BLK %lu %lu\n", k, bhs[k]);
			}
			clock_gettime(CLOCK_MONOTONIC, &t1);
			printf("\n@@@ END %s status %d len %llu hash %llu time %ld\n", name, st, len, h, (long)(t1.tv_sec - t0.tv_sec));
		}
		fflush(stdout);
	}
	printf("@@@ DONE\n");
	fflush(stdout);
	ioctl(1, TIOCDRAIN);
	sync();
	reboot(RB_POWEROFF);
	return 0;
}
