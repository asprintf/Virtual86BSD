/*[SUSv2]*/
#include <xti.h>
#ifdef t_snddis
#undef t_snddis
#endif
int (*foo)(int, const struct t_call *) = t_snddis;
int main(void) { return 0; }
