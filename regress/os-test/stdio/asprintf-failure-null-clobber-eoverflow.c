/* Test whether asprintf sets the output pointer to null on failure, in the case
   that the function is supposed to fail with EOVERFLOW (or ENOMEM). */

#include <sys/mman.h>
#include <sys/resource.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <unistd.h>

#include "suite.h"

static void exhaust_memory(void)
{
	// OpenBSD malloc seems to exit 1 the process if it runs out of memory
	// during the initialization of malloc. Avoid this situation by ensuring
	// the heap has been set up before we trigger an out of memory situation.
#ifdef __OpenBSD__
	malloc(1);
#endif
	// Politely ask the operating system not give us any memory. In theory, this
	// would be enough. This is an important safety to prevent unstable and rude
	// system wide out-of-memory conditions. We only need to simulate it
	// happening in this test, if at all possible.
	struct rlimit limit = { .rlim_cur = 1, .rlim_max = 1 };
#ifdef RLIMIT_AS // Missing on OpenBSD
	setrlimit(RLIMIT_AS, &limit);
#endif
	setrlimit(RLIMIT_DATA, &limit);
	// However, it doesn't seem to have the desired effect on all operating
	// systems, if they aren't implemented properly. The GNU Hurd version I am
	// using seems to not enforce memory limits, so we also try to exhaust
	// virtual memory. If a system does not have overcommit, this could exhaust
	// physical memory. Such systems should have working setrlimit, or they are
	// asking for trouble.
	long page_size = sysconf(_SC_PAGE_SIZE);
	if ( page_size <= 0 )
		page_size = 4096;
	uint64_t exponent = 63;
	while ( exponent )
	{
		uint64_t size = UINT64_C(1) << exponent;
		if ( size < (uint64_t) page_size )
			break;
		if ( SIZE_MAX <= size  )
		{
			exponent--;
			continue;
		}
		// A single system wide zero page can be shared in this case on systems
		// without actually spending any physical memory, even on systems
		// without overcommit.
		void* ptr = mmap(NULL, size, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE,
		                 -1, 0);
		if ( ptr != MAP_FAILED )
			continue;
		exponent--;
	}
	// The process should now be almost out of memory, where only any existing
	// free space space in the malloc heap should be available.
}

int main(void)
{
	if ( getenv("OS_TEST_NO_OOM") && !strcmp(getenv("OS_TEST_NO_OOM"), "1") )
	{
		puts("unknown");
		return 0;
	}
	// Cause a situation where the process is almost out of memory before the
	// big allocation test fails below.
	exhaust_memory();
	// See if this output pointer is unchanged, clobbered to NULL, or
	// clobbered to some other value in the event that asprintf fails.
	char* old = "old";
	char* output = old;
	// Worst case asprintf might try to produce a 8 gigabyte string before
	// realizing it's out of its depth, and the system might trash on low memory
	// conditions. The call is supposed to fail immediately since it would
	// produce way too much output (more than INT_MAX) or fail very quickly
	// because no memory is available. As a safety, add a little timeout.
	alarm(10);
	errno = 0;
	// Don't do this at home.
	int ret = asprintf(&output, "123%*s%*s", INT_MAX, "x", INT_MAX, "x");
	int old_errno = errno;
	// Stop the safety timeout if we didn't die.
	alarm(0);
	// asprintf should never succeed because 3+2*INT_MAX > INT_MAX.
	if ( 0 <= ret )
		errx(2, "asprintf did not fail (returned %d)", ret);
	// Record how asprintf failed.
	puts(strerrno(old_errno));
	// asprintf could leave the old value behind in the output pointer;
	if ( output == old )
		printf("output == old\n");
	// or asprintf could set the output buffer to NULL;
	else if ( output == NULL )
		printf("output == NULL\n");
	// or asprintf could leak one of its own temporary buffers on error.
	else
		printf("output != old && output != NULL\n");
	return 0;
}
