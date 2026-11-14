/*[SUSv2]*/
#include <xti.h>
#ifdef t_sndrel
#undef t_sndrel
#endif
int (*foo)(int) = t_sndrel;
int main(void) { return 0; }
