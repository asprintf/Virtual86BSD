/*[SUSv2]*/
#include <xti.h>
#ifdef t_bind
#undef t_bind
#endif
int (*foo)(int, const struct t_bind *, struct t_bind *) = t_bind;
int main(void) { return 0; }
