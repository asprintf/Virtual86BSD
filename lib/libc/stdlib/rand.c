/* rand/srand: 64-bit LCG (Knuth MMIX constants), upper 31 bits.
 * Thread-safe in the sense of not corrupting state; sequences from
 * concurrent callers interleave, as POSIX allows. */
#include <stdlib.h>
#include <stdint.h>

static uint64_t seed = 1;

void srand(unsigned s) { seed = s - 1; }

int rand(void)
{
	seed = 6364136223846793005ULL * seed + 1;
	return (int)(seed >> 33);
}
