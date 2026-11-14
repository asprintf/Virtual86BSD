/*[SUSv2]*/
#include <xti.h>
#ifdef t_free
#undef t_free
#endif
int (*foo)(void *, int) = t_free;
int main(void) { return 0; }
