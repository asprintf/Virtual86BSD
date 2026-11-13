/*[SUSv2]*/
/* Test whether CLK_TCK is sysconf(_SC_CLK_TCK). */

#include <time.h>
#include <unistd.h>

#include "../basic.h"

int main(void)
{
	if ( (long) CLK_TCK != sysconf(_SC_CLK_TCK) )
		errx(1, "CLK_TCK is %ld, sysconf(_SC_CLK_TCK) %ld", (long) CLK_TCK, sysconf(_SC_CLK_TCK));
	return 0;
}
