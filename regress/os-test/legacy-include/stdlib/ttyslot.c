/*[SUSv2]*/
#include <stdlib.h>
#ifdef ttyslot
#undef ttyslot
#endif
int (*foo)(void) = ttyslot;
int main(void) { return 0; }
