/*[SUSv3]*/
#include <sys/timeb.h>
#ifdef ftime
#undef ftime
#endif
int (*foo)(struct timeb *) = ftime;
int main(void) { return 0; }
