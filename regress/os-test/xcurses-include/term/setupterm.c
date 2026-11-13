/*[SUSv2]*/
#include <term.h>
#ifdef setupterm
#undef setupterm
#endif
int (*foo)(char *, int, int *) = setupterm;
int main(void) { return 0; }
