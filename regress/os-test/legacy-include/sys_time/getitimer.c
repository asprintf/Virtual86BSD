/*[SUSv4]*/
#include <sys/time.h>
#ifdef getitimer
#undef getitimer
#endif
int (*foo)(int, struct itimerval *) = getitimer;
int main(void) { return 0; }
