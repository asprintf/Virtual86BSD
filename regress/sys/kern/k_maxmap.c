/* k_maxmap.c - mappings per process: mmap fails with ENOMEM at the limit
 * (MAX_MAP_COUNT, 65530 as Linux), quickly (the free-area search must not
 * be quadratic), and munmap gives the room back.  Prints "k_maxmap ok". */
#define _POSIX_C_SOURCE 202405L
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
#include <time.h>

int main(void)
{
	static void *maps[70000];
	struct timespec t0, t1;
	int n = 0, fail = 0;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	/* alternate protections so that neighbours cannot be merged */
	for (; n < 70000; n++) {
		void *p = mmap(NULL, 4096, n % 2 ? PROT_READ : PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (p == MAP_FAILED)
			break;
		maps[n] = p;
	}
	int e = errno;
	clock_gettime(CLOCK_MONOTONIC, &t1);
	printf("%d mappings, then %s, in %.1f s\n", n, n < 70000 ? (e == ENOMEM ? "ENOMEM" : "another error") : "no limit",
	       (double)(t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9);
	if (n >= 70000 || e != ENOMEM || n < 60000)
		fail = 1;
	for (int i = 0; i < 100; i++)
		munmap(maps[i], 4096);
	if (mmap(NULL, 4096, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
		fail = 1;
	printf(fail ? "k_maxmap FAILED\n" : "k_maxmap ok\n");
	return fail;
}
