/*[SUSv2]*/
#include <xti.h>
#ifdef t_sync
#undef t_sync
#endif
int (*foo)(int) = t_sync;
int main(void) { return 0; }
