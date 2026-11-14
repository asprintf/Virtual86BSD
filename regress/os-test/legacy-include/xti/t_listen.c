/*[SUSv2]*/
#include <xti.h>
#ifdef t_listen
#undef t_listen
#endif
int (*foo)(int, struct t_call *) = t_listen;
int main(void) { return 0; }
