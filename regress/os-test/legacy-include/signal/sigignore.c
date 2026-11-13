/*[SUSv4]*/
#include <signal.h>
#ifdef sigignore
#undef sigignore
#endif
int (*foo)(int) = sigignore;
int main(void) { return 0; }
