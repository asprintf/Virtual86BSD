/*[SUSv4]*/
#include <sys/time.h>
#ifdef setitimer
#undef setitimer
#endif
int (*foo)(int, const struct itimerval *, struct itimerval *) = setitimer;
int main(void) { return 0; }
