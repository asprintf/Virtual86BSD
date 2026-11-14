/*[SUSv2]*/
#include <xti.h>
#ifdef t_alloc
#undef t_alloc
#endif
void *(*foo)(int, int, int) = t_alloc;
int main(void) { return 0; }
