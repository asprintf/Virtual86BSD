/*[SUSv4]*/
#include <time.h>
#ifdef ctime_r
#undef ctime_r
#endif
char *(*foo)(const time_t *, char *) = ctime_r;
int main(void) { return 0; }
