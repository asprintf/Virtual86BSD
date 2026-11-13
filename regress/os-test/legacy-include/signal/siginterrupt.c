/*[SUSv4]*/
#include <signal.h>
#ifdef siginterrupt
#undef siginterrupt
#endif
int (*foo)(int, int) = siginterrupt;
int main(void) { return 0; }
