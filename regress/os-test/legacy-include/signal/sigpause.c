/*[SUSv4]*/
#include <signal.h>
#ifdef sigpause
#undef sigpause
#endif
int (*foo)(int) = sigpause;
int main(void) { return 0; }
