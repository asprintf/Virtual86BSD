/* k_xcase: IUCLC, OLCUC and XCASE of the line discipline, on a pseudo-terminal. */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s (errno %d)\n", __LINE__, #c, errno); fails++; } } while (0)

static void put(int fd, const char *s)
{
	CHECK(write(fd, s, strlen(s)) == (ssize_t)strlen(s));
}

/* all that arrives on fd, until 300 ms of silence, is WANT */
static int arrives(int fd, const char *want)
{
	char buf[256];
	size_t n = 0;
	struct pollfd p = { fd, POLLIN, 0 };
	while (n < sizeof buf - 1 && poll(&p, 1, 300) > 0) {
		ssize_t r = read(fd, buf + n, sizeof buf - 1 - n);
		if (r <= 0)
			break;
		n += (size_t)r;
	}
	buf[n] = 0;
	if (n == strlen(want) && !memcmp(buf, want, n))
		return 1;
	printf("  got %zu bytes:", n);
	for (size_t i = 0; i < n; i++)
		printf(" %02x", (unsigned char)buf[i]);
	printf("\n");
	return 0;
}

/* a read of the slave gives WANT (a complete line, or what is there) */
static int reads(int fd, const char *want)
{
	char buf[256];
	ssize_t n = read(fd, buf, sizeof buf);
	return n == (ssize_t)strlen(want) && !memcmp(buf, want, (size_t)n);
}

static void flags(int s, tcflag_t iset, tcflag_t iclr, tcflag_t oset, tcflag_t oclr, tcflag_t lset, tcflag_t lclr)
{
	struct termios t;
	tcgetattr(s, &t);
	t.c_iflag = (t.c_iflag | iset) & ~iclr;
	t.c_oflag = (t.c_oflag | oset) & ~oclr;
	t.c_lflag = (t.c_lflag | lset) & ~lclr;
	CHECK(tcsetattr(s, TCSANOW, &t) == 0);
}

int main(void)
{
	int m = posix_openpt(O_RDWR | O_NOCTTY), s;

	CHECK(m >= 0 && grantpt(m) == 0 && unlockpt(m) == 0);
	s = open(ptsname(m), O_RDWR | O_NOCTTY);
	CHECK(s >= 0);

	/* IUCLC: the input is in lower case; not when it is not set */
	flags(s, IUCLC, 0, 0, 0, 0, ECHO);
	put(m, "ABC dE\n");
	CHECK(reads(s, "abc de\n"));
	flags(s, 0, IUCLC, 0, 0, 0, 0);
	put(m, "AbC\n");
	CHECK(reads(s, "AbC\n"));

	/* OLCUC: the output is in upper case, and the rest of the post-processing stays */
	flags(s, 0, 0, OLCUC, 0, 0, 0);
	put(s, "abc XyZ\n");
	CHECK(arrives(m, "ABC XYZ\r\n"));
	flags(s, 0, 0, 0, OLCUC, 0, 0);
	put(s, "abC\n");
	CHECK(arrives(m, "abC\r\n"));

	/* XCASE (in canonical mode): backslash escapes in the input */
	flags(s, IUCLC, 0, OLCUC, 0, XCASE, 0);
	put(m, "\\a\\B C\\'\\(\\)\\!\\^\\\\\n");
	CHECK(reads(s, "AB c`{}|~\\\n"));
	put(m, "\\1\\ \n");                       /* a backslash before anything else stays */
	CHECK(reads(s, "\\1\\ \n"));

	/* XCASE: a backslash before capitals and ` | ~ { } \ in the output */
	put(s, "Ab`{\\x|~}\n");
	CHECK(arrives(m, "\\AB\\'\\(\\\\X\\!\\^\\)\r\n"));

	/* the echo goes the same way */
	flags(s, 0, 0, 0, 0, ECHO, 0);
	put(m, "\\a");
	CHECK(arrives(m, "\\A"));
	put(m, "b\n");
	CHECK(arrives(m, "B\r\n"));
	CHECK(reads(s, "Ab\n"));
	/* and erasing one takes both columns of "\A" */
	put(m, "\\a\177");
	CHECK(arrives(m, "\\A\b \b\b \b"));
	put(m, "\n");
	CHECK(arrives(m, "\r\n"));
	CHECK(reads(s, "\n"));

	/* no XCASE without ICANON; without XCASE a backslash is a character */
	flags(s, 0, 0, 0, 0, 0, ICANON | ECHO);
	put(s, "A`");
	CHECK(arrives(m, "A`"));
	put(m, "\\a");
	CHECK(reads(s, "\\a"));
	flags(s, 0, 0, 0, 0, ICANON, XCASE);
	put(m, "\\a\n");
	CHECK(reads(s, "\\a\n"));

	printf("k_xcase: %s\n", fails ? "FAILED" : "ok");
	return fails != 0;
}
