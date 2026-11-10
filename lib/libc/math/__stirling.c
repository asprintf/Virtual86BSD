#include "libm.h"

/* Stirling's series: lgamma(x) - ((x - 1/2) log x - x + log(2 pi)/2) */
long double __stirling_tail(long double x)
{
	static const long double c[] = {   /* B_2k / (2k (2k - 1)) */
		1.0L/12, -1.0L/360, 1.0L/1260, -1.0L/1680, 1.0L/1188, -691.0L/360360,
		1.0L/156, -3617.0L/122400, 43867.0L/244188, -174611.0L/125400,
		77683.0L/5796, -236364091.0L/1506960,
	};
	long double z = 1 / (x * x), s = 0;
	int k;
	for (k = 11; k >= 0; k--)
		s = s * z + c[k];
	return s / x;
}
