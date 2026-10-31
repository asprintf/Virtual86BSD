#include <stdlib.h>

void __stack_chk_fail(void);

void __stack_chk_fail(void)
{
	abort();
}
