/*[SUSv4]*/
#include <sys/time.h>
#ifdef gettimeofday
#undef gettimeofday
#endif
int (*foo)(struct timeval *, void *) = gettimeofday;
int main(void) { return 0; }
