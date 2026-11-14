/*[SUSv2]*/
#include <xti.h>
#ifdef t_sndvudata
#undef t_sndvudata
#endif
int (*foo)(int, struct t_unitdata *, struct t_iovec *, unsigned int) = t_sndvudata;
int main(void) { return 0; }
