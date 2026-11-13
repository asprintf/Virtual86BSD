/*[SUSv4]*/
#include <signal.h>
#ifdef sigset
#undef sigset
#endif
void (*(*foo)(int, void (*)(int)))(int) = sigset;
int main(void) { return 0; }
