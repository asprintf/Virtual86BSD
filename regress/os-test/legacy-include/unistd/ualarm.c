/*[SUSv3]*/
#include <unistd.h>
#ifdef ualarm
#undef ualarm
#endif
useconds_t (*foo)(useconds_t, useconds_t) = ualarm;
int main(void) { return 0; }
