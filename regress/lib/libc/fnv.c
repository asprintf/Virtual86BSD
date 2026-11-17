/* fnv - the length and FNV-1a hash (64 bits) of a file, as vminit.c prints them;
 * with -b also the hash of each 200 lines ("BLK n hash") */
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv)
{
	unsigned long long h = 14695981039346656037ULL, bh = h, n = 0;
	unsigned long line = 0, blk = 0;
	int c, b = argc > 2 && !strcmp(argv[1], "-b");
	FILE *f = fopen(argv[argc - 1], "rb");
	if (!f) return 1;
	while ((c = getc(f)) != EOF) {
		h = (h ^ (unsigned)c) * 1099511628211ULL;
		bh = (bh ^ (unsigned)c) * 1099511628211ULL;
		n++;
		if (c == '\n' && ++line % 200 == 0) {
			if (b) printf("BLK %lu %llu\n", blk, bh);
			blk++;
			bh = 14695981039346656037ULL;
		}
	}
	if (!b) printf("%llu %llu\n", n, h);
	return 0;
}
