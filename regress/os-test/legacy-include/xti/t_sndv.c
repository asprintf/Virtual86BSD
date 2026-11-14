/*[SUSv2]*/
#include <xti.h>
#ifdef t_sndv
#undef t_sndv
#endif
int (*foo)(int, const struct t_iovec *, unsigned int, int) = t_sndv;
int main(void) { return 0; }
