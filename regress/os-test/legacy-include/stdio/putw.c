/*[SUSv2]*/
#include <stdio.h>
#ifdef putw
#undef putw
#endif
int (*foo)(int, FILE *) = putw;
int main(void) { return 0; }
