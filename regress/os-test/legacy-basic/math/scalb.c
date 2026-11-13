/*[SUSv3]*/
/* Test whether scalb computes x*2^n with the errors SUSv3 lists. */

#include <errno.h>
#include <float.h>
#include <math.h>

#include "../basic.h"

int main(void)
{
	if ( scalb(3.0, 4.0) != 48.0 )
		errx(1, "scalb(3, 4) was %g", scalb(3.0, 4.0));
	if ( scalb(48.0, -4.0) != 3.0 )
		errx(1, "scalb(48, -4) was %g", scalb(48.0, -4.0));
	if ( scalb(5.0, 0.0) != 5.0 )
		errx(1, "scalb(x, 0) was not x");
	if ( !isnan(scalb(NAN, 1.0)) || !isnan(scalb(1.0, NAN)) )
		errx(1, "scalb of a NaN was not a NaN");
	if ( scalb(INFINITY, 2.0) != INFINITY )
		errx(1, "scalb(Inf, 2) was not Inf");
	errno = 0;
	if ( !isnan(scalb(0.0, INFINITY)) || errno != EDOM )
		errx(1, "scalb(0, +Inf) was not a domain error");
	errno = 0;
	if ( !isnan(scalb(INFINITY, -INFINITY)) || errno != EDOM )
		errx(1, "scalb(Inf, -Inf) was not a domain error");
	errno = 0;
	if ( scalb(DBL_MAX, 10.0) != HUGE_VAL || errno != ERANGE )
		errx(1, "scalb overflow was not a range error with HUGE_VAL");
	if ( MAXFLOAT != FLT_MAX )
		errx(1, "MAXFLOAT is not FLT_MAX");
	return 0;
}
