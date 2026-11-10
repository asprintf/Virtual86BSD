#include <strings.h>
int ffsll(long long i)
{
	unsigned long long u = (unsigned long long)i;
	int n = 1;
	if (!u) return 0;
	while (!(u & 1)) { u >>= 1; n++; }
	return n;
}
int ffsl(long i) { return ffsll(i); }
int ffs(int i) { return ffsll((unsigned)i); }
