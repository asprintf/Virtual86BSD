/*[SUSv4]*/
#include <stdlib.h>
#ifdef rand_r
#undef rand_r
#endif
int (*foo)(unsigned int *) = rand_r;
int main(void) { return 0; }
