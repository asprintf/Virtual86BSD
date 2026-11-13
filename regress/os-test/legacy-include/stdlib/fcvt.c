/*[SUSv3]*/
#include <stdlib.h>
#ifdef fcvt
#undef fcvt
#endif
char *(*foo)(double, int, int *, int *) = fcvt;
int main(void) { return 0; }
