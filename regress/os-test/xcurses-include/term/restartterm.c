/*[SUSv2]*/
#include <term.h>
#ifdef restartterm
#undef restartterm
#endif
int (*foo)(char *, int, int *) = restartterm;
int main(void) { return 0; }
