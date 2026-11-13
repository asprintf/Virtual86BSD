/*[SUSv2]*/
#include <unistd.h>
#ifdef sbrk
#undef sbrk
#endif
void *(*foo)(intptr_t) = sbrk;
int main(void) { return 0; }
