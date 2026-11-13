/*[SUSv3]*/
#include <stdlib.h>
#ifdef gcvt
#undef gcvt
#endif
char *(*foo)(double, int, char *) = gcvt;
int main(void) { return 0; }
