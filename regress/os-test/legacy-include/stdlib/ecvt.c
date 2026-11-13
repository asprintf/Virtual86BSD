/*[SUSv3]*/
#include <stdlib.h>
#ifdef ecvt
#undef ecvt
#endif
char *(*foo)(double, int, int *, int *) = ecvt;
int main(void) { return 0; }
