/*[SUSv2]*/
#include <unistd.h>
#ifdef getdtablesize
#undef getdtablesize
#endif
int (*foo)(void) = getdtablesize;
int main(void) { return 0; }
