/*[SUSv2]*/
/* Test whether valloc gives page-aligned memory. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../basic.h"

int main(void)
{
	long page = sysconf(_SC_PAGESIZE);
	char* p = valloc(100);
	if ( !p )
		err(1, "valloc");
	if ( (uintptr_t)p % (uintptr_t)page != 0 )
		errx(1, "valloc's memory %p is not aligned to %ld", (void*)p, page);
	memset(p, 1, 100);
	free(p);
	return 0;
}
