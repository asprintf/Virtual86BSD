#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include "utx_impl.h"

/* Offsets in the 384-byte record. */
enum { O_TYPE = 0, O_PID = 4, O_LINE = 8, O_ID = 40, O_USER = 44, O_HOST = 76,
       O_TERM = 332, O_EXIT = 334, O_SESS = 336, O_SEC = 340, O_USEC = 344, O_ADDR = 348 };

static const char *const paths[] = { "/run/utmp", "/var/run/utmp" };

/* The database file, or -1.  Opened for writing (pututxline), a database
   that does not exist yet is created, in the first of the two directories
   that exists: POSIX has pututxline write the entry when the process has
   the privileges (here: may create the file there). */
int __utx_open(int flags)
{
	unsigned i;
	int fd, e = errno;
	for (i = 0; i < sizeof paths / sizeof *paths; i++)
		if ((fd = open(paths[i], flags | O_CLOEXEC)) >= 0) return fd;
	if ((flags & O_ACCMODE) == O_RDONLY || errno != ENOENT) return -1;
	for (i = 0; i < sizeof paths / sizeof *paths; i++) {
		errno = e;
		if ((fd = open(paths[i], flags | O_CREAT | O_CLOEXEC, 0644)) >= 0) return fd;
		if (errno != ENOENT) return -1;     /* the directory exists: not allowed */
	}
	return -1;
}

static int32_t g32(const unsigned char *p) { int32_t v; memcpy(&v, p, 4); return v; }
static int16_t g16(const unsigned char *p) { int16_t v; memcpy(&v, p, 2); return v; }
static void p32(unsigned char *p, int32_t v) { memcpy(p, &v, 4); }
static void p16(unsigned char *p, int16_t v) { memcpy(p, &v, 2); }

void __utx_decode(struct utmpx *u, const unsigned char *r)
{
	memset(u, 0, sizeof *u);
	u->ut_type = g16(r + O_TYPE);
	u->ut_pid = g32(r + O_PID);
	memcpy(u->ut_line, r + O_LINE, 32);
	memcpy(u->ut_id, r + O_ID, 4);
	memcpy(u->ut_user, r + O_USER, 32);
	memcpy(u->ut_host, r + O_HOST, 256);
	u->ut_exit.__e_termination = g16(r + O_TERM);
	u->ut_exit.__e_exit = g16(r + O_EXIT);
	u->ut_session = g32(r + O_SESS);
	/* 32-bit seconds are unsigned here, which postpones 2038 to 2106 */
	u->ut_tv.tv_sec = (long)(uint32_t)g32(r + O_SEC);
	u->ut_tv.tv_usec = g32(r + O_USEC);
	memcpy(u->ut_addr_v6, r + O_ADDR, 16);
}

void __utx_encode(unsigned char *r, const struct utmpx *u)
{
	memset(r, 0, UTX_RECSIZE);
	p16(r + O_TYPE, u->ut_type);
	p32(r + O_PID, u->ut_pid);
	memcpy(r + O_LINE, u->ut_line, 32);
	memcpy(r + O_ID, u->ut_id, 4);
	memcpy(r + O_USER, u->ut_user, 32);
	memcpy(r + O_HOST, u->ut_host, 256);
	p16(r + O_TERM, u->ut_exit.__e_termination);
	p16(r + O_EXIT, u->ut_exit.__e_exit);
	p32(r + O_SESS, u->ut_session);
	p32(r + O_SEC, (int32_t)(uint32_t)u->ut_tv.tv_sec);
	p32(r + O_USEC, (int32_t)u->ut_tv.tv_usec);
	memcpy(r + O_ADDR, u->ut_addr_v6, 16);
}
