/*[SUSv4]*/
#include <time.h>
#ifdef asctime_r
#undef asctime_r
#endif
char *(*foo)(const struct tm *, char *) = asctime_r;
int main(void) { return 0; }
