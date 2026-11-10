/* Self-checking: typed memory objects ([TYM]), where the system has them
 * (Shall's kernel: pools ram0 and ram1; elsewhere the functions must fail
 * as for no typed memory). */
#define _XOPEN_SOURCE 800
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "out.h"

#define K 1024

static size_t avail(int fd)
{
	struct posix_typed_mem_info i;
	return posix_typed_mem_get_info(fd, &i) == 0 ? i.posix_tmi_length : (size_t)-1;
}

int main(void)
{
	int fa, fc, fn, fm, fd;
	off_t off, off2;
	size_t cl;
	char *p, *q, *r, *anon;

	anon = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	CHECK(posix_mem_offset(anon, 4096, &off, &cl, &fd) == EACCES);   /* not typed memory */
	if (sysconf(_SC_TYPED_MEMORY_OBJECTS) < 0) {
		errno = 0;
		CHECK(posix_typed_mem_open("/ram0", O_RDWR, 0) == -1 && errno == ENOENT);
		os("typed memory: none here: not tested\n");
		os(failures ? "failures\n" : "ok\n");
		oflush();
		return failures != 0;
	}
	CHECK(sysconf(_SC_TYPED_MEMORY_OBJECTS) == 202405L);
	errno = 0;
	CHECK(posix_typed_mem_open("/ram0", O_RDWR, POSIX_TYPED_MEM_ALLOCATE | POSIX_TYPED_MEM_ALLOCATE_CONTIG) == -1 &&
	      errno == EINVAL);
	errno = 0;
	CHECK(posix_typed_mem_open("/nosuch", O_RDWR, 0) == -1 && errno == ENOENT);

	fa = posix_typed_mem_open("/ram0", O_RDWR | O_CLOEXEC, POSIX_TYPED_MEM_ALLOCATE);
	fc = posix_typed_mem_open("/ram0", O_RDWR, POSIX_TYPED_MEM_ALLOCATE_CONTIG);
	fn = posix_typed_mem_open("/ram0", O_RDWR, 0);
	fm = posix_typed_mem_open("/ram0", O_RDWR, POSIX_TYPED_MEM_MAP_ALLOCATABLE);
	CHECK(fa >= 0 && fc >= 0 && fn >= 0 && fm >= 0);
	CHECK((fcntl(fa, F_GETFD) & FD_CLOEXEC) && !(fcntl(fc, F_GETFD) & FD_CLOEXEC));
	size_t total = avail(fn), a0 = avail(fa);
	CHECK(total >= 1024 * K && a0 == total && avail(fc) == total);
	errno = 0;
	{
		struct posix_typed_mem_info i;
		CHECK(posix_typed_mem_get_info(1, &i) == ENODEV);
	}

	/* allocating: the pool shrinks while mapped; posix_mem_offset finds it */
	p = mmap(0, 64 * K, PROT_READ | PROT_WRITE, MAP_SHARED, fa, 0);
	CHECK(p != MAP_FAILED);
	memset(p, 'x', 64 * K);
	CHECK(avail(fa) == a0 - 64 * K);
	CHECK(posix_mem_offset(p + 100, 64 * K, &off, &cl, &fd) == 0 && off % 4096 == 100 && fd >= 0 && cl > 0);
	/* contiguous allocation: all of it in one run */
	q = mmap(0, 128 * K, PROT_READ | PROT_WRITE, MAP_SHARED, fc, 0);
	CHECK(q != MAP_FAILED);
	CHECK(posix_mem_offset(q, 128 * K, &off2, &cl, &fd) == 0 && cl == 128 * K);
	/* the same memory through a non-allocating descriptor, at its offset */
	r = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fn, off - off % 4096);
	CHECK(r != MAP_FAILED && r[100] == 'x');
	r[101] = 'Y';
	CHECK(p[101] == 'Y');
	munmap(r, 4096);
	/* a mapping without allocation keeps its pages from being allocated,
	   unless with POSIX_TYPED_MEM_MAP_ALLOCATABLE */
	{
		off_t top = (off_t)total - 64 * K;           /* (not yet allocated: allocation goes from the bottom) */
		size_t before = avail(fa);
		char *m = mmap(0, 64 * K, PROT_READ, MAP_SHARED, fm, top);
		CHECK(m != MAP_FAILED && avail(fa) == before);
		char *n = mmap(0, 64 * K, PROT_READ, MAP_SHARED, fn, top);
		CHECK(n != MAP_FAILED && avail(fa) == before - 64 * K);
		munmap(n, 64 * K);
		CHECK(avail(fa) == before);
		munmap(m, 64 * K);
		errno = 0;
		CHECK(mmap(0, 4096, PROT_READ, MAP_SHARED, fn, (off_t)total) == MAP_FAILED && errno == ENXIO);
	}
	/* shared with a child */
	pid_t pid = fork();
	if (pid == 0) {
		p[0] = 'C';
		_exit(0);
	}
	{
		int st;
		waitpid(pid, &st, 0);
		CHECK(p[0] == 'C');
	}
	/* unmapped: back in the pool */
	munmap(p, 64 * K);
	munmap(q, 128 * K);
	CHECK(avail(fa) == a0);
	/* too much */
	errno = 0;
	CHECK(mmap(0, total + 4096, PROT_READ, MAP_SHARED, fa, 0) == MAP_FAILED && errno == ENOMEM);
	/* the other pool */
	fd = posix_typed_mem_open("ram1", O_RDONLY, POSIX_TYPED_MEM_ALLOCATE);
	CHECK(fd >= 0 && avail(fd) > 0 && avail(fd) < total);
	close(fd);
	close(fa), close(fc), close(fn), close(fm);
	os(failures ? "failures\n" : "ok\n");
	oflush();
	return failures != 0;
}
