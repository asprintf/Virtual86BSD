/*[SUSv4]*/
#include <ulimit.h>
#ifdef ulimit
#undef ulimit
#endif
long (*foo)(int, ...) = ulimit;
int main(void) { return 0; }
