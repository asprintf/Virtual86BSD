/*[SUSv4]*/
/* Test whether rand_r repeats its sequence for the same seed. */

#include <stdlib.h>

#include "../basic.h"

int main(void)
{
	unsigned a = 42, b = 42;
	int first[16];
	for ( int i = 0; i < 16; i++ )
	{
		first[i] = rand_r(&a);
		if ( first[i] < 0 || RAND_MAX < first[i] )
			errx(1, "rand_r was out of range: %d", first[i]);
	}
	for ( int i = 0; i < 16; i++ )
		if ( rand_r(&b) != first[i] )
			errx(1, "rand_r gave another sequence for the same seed");
	int same = 1;
	for ( int i = 1; i < 16; i++ )
		same &= first[i] == first[0];
	if ( same )
		errx(1, "rand_r gave the same number 16 times");
	return 0;
}
