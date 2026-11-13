/*[SUSv2]*/
#include <unistd.h>
#ifdef brk
#undef brk
#endif
int (*foo)(void *) = brk;
int main(void) { return 0; }
