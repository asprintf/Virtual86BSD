/*[SUSv2]*/
#include <stdio.h>
#ifdef cuserid
#undef cuserid
#endif
char *(*foo)(char *) = cuserid;
int main(void) { return 0; }
