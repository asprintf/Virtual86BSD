/*[SUSv2]*/
#include <unistd.h>
#ifdef getpagesize
#undef getpagesize
#endif
int (*foo)(void) = getpagesize;
int main(void) { return 0; }
