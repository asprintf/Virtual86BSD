/* k_net: TCP, UDP and ICMP through the network card.  Meant for QEMU's
   user networking, with a helper on the host (see README, Testing): a TCP
   echo server on the host's port T, a UDP echo server on U (reached as
   10.0.2.2), and a client that connects to this program's server on port
   7777 through a forwarded port.  Usage: k_net T U */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s (errno %d)\n", __LINE__, #c, errno); fails++; } } while (0)

static struct sockaddr_in host(int port)
{
	struct sockaddr_in s;
	memset(&s, 0, sizeof s);
	s.sin_family = AF_INET;
	s.sin_port = htons((unsigned short)port);
	s.sin_addr.s_addr = inet_addr("10.0.2.2");
	return s;
}

static void timeout(int fd, int sec)
{
	struct timeval tv = {sec, 0};
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
}

#define BIG (2 * 1024 * 1024)

int main(int argc, char **argv)
{
	int tport, uport, fd, st;
	char buf[8192];
	struct sockaddr_in sa, pa;
	socklen_t sl;

	if (argc < 3)
		return 2;
	tport = atoi(argv[1]);
	uport = atoi(argv[2]);
	signal(SIGPIPE, SIG_IGN);

	/* the interface */
	CHECK(if_nametoindex("em0") == 2);
	{
		struct { char name[16]; struct sockaddr_in addr; char pad[8]; } r;   /* Linux's struct ifreq */
		memset(&r, 0, sizeof r);
		strcpy(r.name, "em0");
		fd = socket(AF_INET, SOCK_DGRAM, 0);
		CHECK(ioctl(fd, 0x8915 /* SIOCGIFADDR */, &r) == 0);
		printf("em0 %s\n", inet_ntoa(r.addr.sin_addr));
		close(fd);
	}

	/* UDP echo */
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	timeout(fd, 5);
	sa = host(uport);
	CHECK(sendto(fd, "hello udp", 9, 0, (struct sockaddr *)&sa, sizeof sa) == 9);
	sl = sizeof pa;
	CHECK(recvfrom(fd, buf, sizeof buf, 0, (struct sockaddr *)&pa, &sl) == 9 && !memcmp(buf, "hello udp", 9) &&
	      pa.sin_addr.s_addr == sa.sin_addr.s_addr && pa.sin_port == sa.sin_port);
	/* large datagrams: fragmented both ways (the MTU is 1500) */
	{
		static char d[9000], e[9000];
		for (int i = 0; i < (int)sizeof d; i++)
			d[i] = (char)(i * 13);
		CHECK(sendto(fd, d, 1400, 0, (struct sockaddr *)&sa, sizeof sa) == 1400);
		CHECK(recv(fd, e, sizeof e, 0) == 1400 && !memcmp(d, e, 1400));
		CHECK(sendto(fd, d, 8000, 0, (struct sockaddr *)&sa, sizeof sa) == 8000);
		CHECK(recv(fd, e, sizeof e, 0) == 8000 && !memcmp(d, e, 8000));
	}
	close(fd);

	/* TCP: 2 MiB to the echo server and back, written and read at once */
	fd = socket(AF_INET, SOCK_STREAM, 0);
	sa = host(tport);
	CHECK(connect(fd, (struct sockaddr *)&sa, sizeof sa) == 0);
	sl = sizeof pa;
	CHECK(getsockname(fd, (struct sockaddr *)&pa, &sl) == 0 && pa.sin_addr.s_addr == inet_addr("10.0.2.15"));
	sl = sizeof pa;
	CHECK(getpeername(fd, (struct sockaddr *)&pa, &sl) == 0 && pa.sin_port == htons((unsigned short)tport));
	pid_t pid = fork();
	if (pid == 0) {
		char b[8192];
		long sent = 0;
		while (sent < BIG) {
			for (int i = 0; i < (int)sizeof b; i++)
				b[i] = (char)((sent + i) * 7);
			ssize_t n = write(fd, b, sizeof b);
			if (n <= 0)
				_exit(1);
			sent += n;
		}
		shutdown(fd, SHUT_WR);
		_exit(0);
	}
	{
		long got = 0, bad = 0;
		ssize_t n;
		timeout(fd, 30);
		while ((n = read(fd, buf, sizeof buf)) > 0) {
			for (ssize_t i = 0; i < n; i++)
				bad += buf[i] != (char)((got + i) * 7);
			got += n;
		}
		printf("tcp: %ld bytes back, %ld wrong\n", got, bad);
		CHECK(got == BIG && bad == 0);
		waitpid(pid, &st, 0);
		CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0);
	}
	close(fd);

	/* refused: blocking and non-blocking connect; after a refusal the
	   socket may connect again (as on Linux), and succeeds */
	fd = socket(AF_INET, SOCK_STREAM, 0);
	sa = host(1);
	CHECK(connect(fd, (struct sockaddr *)&sa, sizeof sa) == -1 && errno == ECONNREFUSED);
	sa = host(tport);
	CHECK(connect(fd, (struct sockaddr *)&sa, sizeof sa) == 0 && write(fd, "x", 1) == 1 && read(fd, buf, 1) == 1);
	close(fd);
	sa = host(1);
	fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	CHECK(connect(fd, (struct sockaddr *)&sa, sizeof sa) == -1 && errno == EINPROGRESS);
	{
		struct pollfd pf = {fd, POLLOUT, 0};
		int e = -1;
		CHECK(poll(&pf, 1, 5000) == 1);
		sl = sizeof e;
		CHECK(getsockopt(fd, SOL_SOCKET, SO_ERROR, &e, &sl) == 0 && e == ECONNREFUSED);
	}
	close(fd);
	/* a non-blocking connect that succeeds */
	fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	sa = host(tport);
	CHECK(connect(fd, (struct sockaddr *)&sa, sizeof sa) == -1 && errno == EINPROGRESS);
	{
		struct pollfd pf = {fd, POLLOUT, 0};
		int e = -1;
		CHECK(poll(&pf, 1, 5000) == 1 && (pf.revents & POLLOUT));
		sl = sizeof e;
		CHECK(getsockopt(fd, SOL_SOCKET, SO_ERROR, &e, &sl) == 0 && e == 0);
		CHECK(connect(fd, (struct sockaddr *)&sa, sizeof sa) == -1 && errno == EISCONN);
	}
	close(fd);

	/* ICMP echo to the gateway (QEMU's user networking may not answer) */
	{
		int r = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP), got = 0;
		unsigned char p[16] = {8, 0, 0, 0, 0x42, 0x42, 0, 1, 'p', 'i', 'n', 'g'}, in[128];
		unsigned sum = 0;
		for (int i = 0; i < 16; i += 2)
			sum += (unsigned)(p[i] << 8 | p[i + 1]);
		while (sum >> 16)
			sum = (sum & 0xffff) + (sum >> 16);
		sum = ~sum & 0xffff;
		p[2] = (unsigned char)(sum >> 8), p[3] = (unsigned char)sum;
		sa = host(0);
		timeout(r, 2);
		CHECK(sendto(r, p, 16, 0, (struct sockaddr *)&sa, sizeof sa) == 16);
		for (int i = 0; i < 5 && !got; i++) {
			ssize_t n = recv(r, in, sizeof in, 0);
			if (n < 0)
				break;
			if (n >= 28 && in[9] == 1 && in[20] == 0 && in[24] == 0x42)
				got = 1;
		}
		printf("ping 10.0.2.2: %s\n", got ? "reply" : "no reply");
		close(r);
	}

	/* a server: the host connects through the forwarded port */
	{
		int l = socket(AF_INET, SOCK_STREAM, 0), v = 1, c;
		setsockopt(l, SOL_SOCKET, SO_REUSEADDR, &v, sizeof v);
		memset(&sa, 0, sizeof sa);
		sa.sin_family = AF_INET;
		sa.sin_port = htons(7777);
		CHECK(bind(l, (struct sockaddr *)&sa, sizeof sa) == 0 && listen(l, 4) == 0);
		printf("listening\n");
		fflush(stdout);
		struct pollfd pf = {l, POLLIN, 0};
		CHECK(poll(&pf, 1, 60000) == 1);
		sl = sizeof pa;
		c = accept(l, (struct sockaddr *)&pa, &sl);
		CHECK(c >= 0);
		if (c >= 0) {
			ssize_t n;
			int k = 0;
			timeout(c, 10);
			while (k < (int)sizeof buf - 1 && (n = read(c, buf + k, 1)) == 1 && buf[k] != '\n')
				k++;
			buf[k] = 0;
			printf("from the host: %s\n", buf);
			for (int i = 0; i < k; i++)
				if (buf[i] >= 'a' && buf[i] <= 'z')
					buf[i] = (char)(buf[i] - 32);
			buf[k++] = '\n';
			CHECK(write(c, buf, (size_t)k) == k);
			close(c);
		}
		close(l);
	}

	printf("k_net: %s\n", fails ? "FAILED" : "ok");
	return fails != 0;
}
