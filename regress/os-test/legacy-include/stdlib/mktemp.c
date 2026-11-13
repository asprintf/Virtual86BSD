/*[SUSv3]*/
#include <stdlib.h>
#ifdef mktemp
#undef mktemp
#endif
char *(*foo)(char *) = mktemp;
int main(void) { return 0; }
