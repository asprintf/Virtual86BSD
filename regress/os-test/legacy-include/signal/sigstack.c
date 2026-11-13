/*[SUSv2]*/
#include <signal.h>
#ifdef sigstack
#undef sigstack
#endif
int (*foo)(struct sigstack *, struct sigstack *) = sigstack;
int main(void) { return 0; }
