/*[SUSv2]*/
#include <stdio.h>
#ifdef getw
#undef getw
#endif
int (*foo)(FILE *) = getw;
int main(void) { return 0; }
