#include <utmpx.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "utx_impl.h"

static int fd = -1, rw;
static struct utmpx cur;        /* the entry last returned */
static off_t curpos = -1;       /* its offset, or -1 */

void setutxent(void)
{
	if (fd >= 0) lseek(fd, 0, SEEK_SET);
	curpos = -1;
}

void endutxent(void)
{
	if (fd >= 0) close(fd);
	fd = -1; rw = 0; curpos = -1;
}

/* no database (no utmpx file) is an empty one: errno is kept */
static int opendb(void)
{
	int e = errno;
	if (fd < 0 && (fd = __utx_open(O_RDONLY)) < 0) {
		if (errno == ENOENT) errno = e;
		return -1;
	}
	return 0;
}

static int readrec(struct utmpx *u, off_t *pos)
{
	unsigned char r[UTX_RECSIZE];
	off_t p = lseek(fd, 0, SEEK_CUR);
	if (read(fd, r, sizeof r) != (ssize_t)sizeof r) return -1;
	__utx_decode(u, r);
	if (pos) *pos = p;
	return 0;
}

struct utmpx *getutxent(void)
{
	if (opendb() || readrec(&cur, &curpos)) { curpos = -1; return 0; }
	return &cur;
}

static int idmatch(const struct utmpx *id, const struct utmpx *u)
{
	switch (id->ut_type) {
	case BOOT_TIME: case OLD_TIME: case NEW_TIME:
		return u->ut_type == id->ut_type;
	case INIT_PROCESS: case LOGIN_PROCESS: case USER_PROCESS: case DEAD_PROCESS:
		return u->ut_type >= INIT_PROCESS && u->ut_type <= DEAD_PROCESS &&
		       !strncmp(u->ut_id, id->ut_id, sizeof u->ut_id);
	}
	return 0;
}

struct utmpx *getutxid(const struct utmpx *id)
{
	if (opendb()) return 0;
	while (!readrec(&cur, &curpos))
		if (idmatch(id, &cur)) return &cur;
	curpos = -1;
	return 0;
}

struct utmpx *getutxline(const struct utmpx *line)
{
	if (opendb()) return 0;
	while (!readrec(&cur, &curpos))
		if ((cur.ut_type == LOGIN_PROCESS || cur.ut_type == USER_PROCESS) &&
		    !strncmp(cur.ut_line, line->ut_line, sizeof cur.ut_line))
			return &cur;
	curpos = -1;
	return 0;
}

struct utmpx *pututxline(const struct utmpx *u)
{
	static struct utmpx out;
	unsigned char r[UTX_RECSIZE];
	struct utmpx t;
	struct flock fl = { 0 };
	off_t pos = -1, p;
	int nfd;

	if (!rw) {
		if ((nfd = __utx_open(O_RDWR)) < 0) return 0;
		if (fd >= 0) {
			lseek(nfd, lseek(fd, 0, SEEK_CUR), SEEK_SET);
			close(fd);
		}
		fd = nfd; rw = 1;
	}
	out = *u;
	fl.l_type = F_WRLCK; fl.l_whence = SEEK_SET;
	if (fcntl(fd, F_SETLKW, &fl) < 0) return 0;
	/* the entry last read, if it matches, else search the whole file */
	if (curpos >= 0 && idmatch(&out, &cur)) pos = curpos;
	else {
		lseek(fd, 0, SEEK_SET);
		while (!readrec(&t, &p))
			if (idmatch(&out, &t)) { pos = p; break; }
	}
	if (pos < 0) pos = lseek(fd, 0, SEEK_END) / UTX_RECSIZE * UTX_RECSIZE;
	__utx_encode(r, &out);
	if (pwrite(fd, r, sizeof r, pos) != (ssize_t)sizeof r) pos = -1;
	else lseek(fd, pos + UTX_RECSIZE, SEEK_SET);
	fl.l_type = F_UNLCK;
	fcntl(fd, F_SETLK, &fl);
	curpos = -1;
	return pos < 0 ? 0 : &out;
}
