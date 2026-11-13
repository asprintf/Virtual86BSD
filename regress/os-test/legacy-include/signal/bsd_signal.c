/*[SUSv3]*/
#include <signal.h>
#ifdef bsd_signal
#undef bsd_signal
#endif
void (*(*foo)(int, void (*)(int)))(int) = bsd_signal;
int main(void) { return 0; }
