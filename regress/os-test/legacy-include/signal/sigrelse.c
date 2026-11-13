/*[SUSv4]*/
#include <signal.h>
#ifdef sigrelse
#undef sigrelse
#endif
int (*foo)(int) = sigrelse;
int main(void) { return 0; }
