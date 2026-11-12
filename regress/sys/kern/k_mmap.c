/* k_mmap: MAP_SHARED mappings of files on a disk file system (the page cache).  POSIX:
   a store through a shared mapping is seen by every other mapping and by
   read(), a write() by the mappings; msync writes it to the file (and sets
   st_mtime); whole pages beyond the end of the file are SIGBUS; a private
   mapping's stores stay its own.  Run in a directory on one; with "-k"
   the file stays (to check it from outside afterwards). */
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
#define PG 4096

static sigjmp_buf jb;
static volatile int got;
static void onbus(int s) { got = s; siglongjmp(jb, 1); }

/* does reading *p raise SIGBUS? */
static int bus(volatile char *p)
{
	got = 0;
	if (!sigsetjmp(jb, 1))
		(void)*p;
	return got == SIGBUS;
}

int main(int argc, char **argv)
{
	const char *name = "k_mmap.dat";
	char buf[3 * PG], c;
	int fd, st;
	struct stat sb;
	struct sigaction sa;

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = onbus;
	sigaction(SIGBUS, &sa, 0);

	unlink(name);
	fd = open(name, O_RDWR | O_CREAT | O_TRUNC, 0644);
	CHECK(fd >= 0);
	memset(buf, 'a', sizeof buf);
	CHECK(write(fd, buf, 2 * PG + 100) == 2 * PG + 100);   /* 3 pages, the last partial */

	char *m = mmap(0, 4 * PG, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	char *m2 = mmap(0, 3 * PG, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	char *pr = mmap(0, 3 * PG, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	CHECK(m != MAP_FAILED && m2 != MAP_FAILED && pr != MAP_FAILED);

	/* two mappings in one process are the same memory */
	m[10] = 'X';
	CHECK(m2[10] == 'X');
	/* read() sees the store before any msync */
	CHECK(pread(fd, &c, 1, 10) == 1 && c == 'X');
	/* write() is seen by the mappings */
	CHECK(pwrite(fd, "Y", 1, PG + 5) == 1 && m[PG + 5] == 'Y' && m2[PG + 5] == 'Y');
	/* the partial last page: zero beyond the end; the next page is SIGBUS */
	CHECK(m[2 * PG + 99] == 'a' && m[2 * PG + 100] == 0);
	CHECK(bus(m + 3 * PG));

	/* another process, after fork and through its own mapping */
	pid_t p = fork();
	if (p == 0) {
		int f2 = open(name, O_RDWR);
		char *q = mmap(0, 3 * PG, PROT_READ | PROT_WRITE, MAP_SHARED, f2, 0);
		q[20] = 'C';    /* its own mapping */
		m[30] = 'F';    /* the inherited one */
		_exit(q[10] != 'X');
	}
	waitpid(p, &st, 0);
	CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0);
	CHECK(m[20] == 'C' && m2[30] == 'F');
	CHECK(pread(fd, &c, 1, 20) == 1 && c == 'C');

	/* a private mapping: copy on write, its stores not in the file */
	pr[40] = 'P';
	CHECK(m[40] == 'a' && pread(fd, &c, 1, 40) == 1 && c == 'a');

	/* msync: st_mtime moves, the file has the data through a new descriptor */
	fstat(fd, &sb);
	time_t before = sb.st_mtime;
	struct timespec ts = { 1, 100000000 };
	nanosleep(&ts, 0);
	m[50] = 'M';
	CHECK(msync(m, 3 * PG, MS_SYNC) == 0);
	fstat(fd, &sb);
	CHECK(sb.st_mtime > before);
	{
		int f3 = open(name, O_RDONLY);
		CHECK(pread(f3, buf, 64, 0) == 64 && buf[10] == 'X' && buf[20] == 'C' && buf[30] == 'F' && buf[50] == 'M');
		close(f3);
	}

	/* growing the file makes the page reachable; shrinking takes it away */
	CHECK(ftruncate(fd, 4 * PG) == 0);
	CHECK(!bus(m + 3 * PG) && m[3 * PG] == 0);
	m[3 * PG + 1] = 'G';
	CHECK(pread(fd, &c, 1, 3 * PG + 1) == 1 && c == 'G');
	CHECK(ftruncate(fd, PG + 10) == 0);
	CHECK(bus(m + 2 * PG) && bus(m2 + 2 * PG));
	CHECK(m[PG + 9] == 'a' && m[PG + 10] == 0);
	CHECK(ftruncate(fd, 2 * PG) == 0);
	CHECK(m[PG + 10] == 0);           /* not the old contents again */

	/* everything unmapped and closed: the data is in the file */
	munmap(m, 4 * PG);
	munmap(m2, 3 * PG);
	munmap(pr, 3 * PG);
	close(fd);
	fd = open(name, O_RDONLY);
	CHECK(read(fd, buf, PG) == PG && buf[10] == 'X' && buf[20] == 'C' && buf[50] == 'M' && buf[40] == 'a');
	close(fd);

	/* an unlinked file stays mapped */
	fd = open(name, O_RDWR);
	m = mmap(0, PG, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (argc < 2 || strcmp(argv[1], "-k")) {
		unlink(name);
		m[1] = 'U';
		CHECK(m[1] == 'U' && pread(fd, &c, 1, 1) == 1 && c == 'U');
	} else {
		m[1] = 'K';                   /* left for a check from outside */
	}
	munmap(m, PG);
	close(fd);

	/* private mappings map the page cache's pages until they write: a
	   write() shows in a page not yet written (as on Linux; POSIX leaves
	   it open), a store stays the mapping's own, and neither mprotect
	   nor fork lets one process write the pages others see */
	fd = open(name, O_RDWR | O_CREAT | O_TRUNC, 0644);
	CHECK(fd >= 0);
	memset(buf, 'b', sizeof buf);
	CHECK(write(fd, buf, 2 * PG) == 2 * PG);
	char *r1 = mmap(0, 2 * PG, PROT_READ, MAP_PRIVATE, fd, 0);
	char *r2 = mmap(0, 2 * PG, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	CHECK(r1 != MAP_FAILED && r2 != MAP_FAILED);
	CHECK(r1[0] == 'b' && r2[0] == 'b');
	CHECK(pwrite(fd, "W", 1, 0) == 1 && r1[0] == 'W' && r2[0] == 'W');
	r2[1] = 'S';
	CHECK(r1[1] == 'b' && pread(fd, &c, 1, 1) == 1 && c == 'b');
	CHECK(pwrite(fd, "V", 1, 2) == 1 && r1[2] == 'V' && r2[2] == 'b');
	CHECK(mprotect(r1, PG, PROT_READ | PROT_WRITE) == 0);
	r1[3] = 'T';
	CHECK(pread(fd, &c, 1, 3) == 1 && c == 'b' && r2[3] == 'b');
	CHECK(r1[PG] == 'b');             /* mapped in the parent before fork */
	p = fork();
	if (p == 0) {
		int bad = mprotect(r1 + PG, PG, PROT_READ | PROT_WRITE) != 0;
		r1[PG] = 'K';
		r2[PG + 1] = 'K';
		_exit(bad || r1[PG] != 'K');
	}
	waitpid(p, &st, 0);
	CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0);
	CHECK(r1[PG] == 'b' && r2[PG + 1] == 'b' && pread(fd, &c, 1, PG) == 1 && c == 'b');
	{
		/* a new mapping of the file sees the file, not the stores */
		char *r3 = mmap(0, 2 * PG, PROT_READ, MAP_PRIVATE, fd, 0);
		CHECK(r3 != MAP_FAILED && r3[1] == 'b' && r3[3] == 'b' && r3[PG] == 'b');
		munmap(r3, 2 * PG);
	}
	/* pages beyond the end of the file: SIGBUS, for private mappings
	   too, also where a page was copied (as on Linux) */
	r2[PG + 2] = 'Z';
	CHECK(ftruncate(fd, PG) == 0);
	CHECK(bus(r1 + PG) && bus(r2 + PG) && r2[2] == 'b');
	munmap(r1, 2 * PG);
	munmap(r2, 2 * PG);
	close(fd);
	if (argc < 2 || strcmp(argv[1], "-k"))
		unlink(name);

	/* write without PROT_WRITE in a read-only open: EACCES */
	fd = open("/etc/passwd", O_RDONLY);
	CHECK(mmap(0, PG, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) == MAP_FAILED);
	close(fd);

	printf("k_mmap: %s\n", fails ? "FAILED" : "ok");
	return fails != 0;
}
