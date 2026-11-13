/*[SUSv2]*/
/* Test whether gamma is lgamma, signgam included. */

#include <math.h>

#include "../basic.h"

int main(void)
{
	double r = gamma(5.0);
	if ( fabs(r - log(24.0)) > 1e-12 )
		errx(1, "gamma(5) was %.17g, not log(24)", r);
	if ( signgam != 1 )
		errx(1, "gamma(5) set signgam to %d", signgam);
	gamma(-0.5);
	if ( signgam != -1 )
		errx(1, "gamma(-0.5) set signgam to %d, not -1", signgam);
	return 0;
}
