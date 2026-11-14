/*[SUSv2]*/
#include <xti.h>
#ifdef t_sndreldata
#undef t_sndreldata
#endif
int (*foo)(int, struct t_discon *) = t_sndreldata;
int main(void) { return 0; }
