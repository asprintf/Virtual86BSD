/*[SUSv4]*/
#include <utime.h>
#ifdef utime
#undef utime
#endif
int (*foo)(const char *, const struct utimbuf *) = utime;
int main(void) { return 0; }
