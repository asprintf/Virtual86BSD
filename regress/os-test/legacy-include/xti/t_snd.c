/*[SUSv2]*/
#include <xti.h>
#ifdef t_snd
#undef t_snd
#endif
int (*foo)(int, void *, unsigned int, int) = t_snd;
int main(void) { return 0; }
